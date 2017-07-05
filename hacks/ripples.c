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
 * 09 Oct 2016: Updated for new xshm.c
 *
 */

#include <math.h>
#include "screenhack.h"

typedef enum {ripple_drop, ripple_blob, ripple_box, ripple_stir} ripple_mode;

#include "xshm.h"

#define TABLE 256

struct state {
  Display *dpy;
  Window window;
  GC gc;
  Visual *visual;

  XImage *orig_map, *buffer_map;
  int ctab[256];
  Colormap colormap;
  Screen *screen;
  int ncolors;
  int light;

  int width, height; /* ripple size */
  int bigwidth, bigheight; /* screen size */

  Bool transparent;
  short *bufferA, *bufferB, *temp;
  char *dirty_buffer;

  double cos_tab[TABLE];

  Bool grayscale_p;

  unsigned long rmask;  /* This builds on the warp effect by adding */
  unsigned long gmask;  /* in a lighting effect: brighten pixels by an */
  unsigned long bmask;  /* amount corresponding to the vertical gradient */

  int rshift;
  int gshift;
  int bshift;

  double stir_ang;

  int draw_toggle;
  int draw_count;

  int iterations, delay, rate, box, oily, stir, fluidity;
  int duration;
  time_t start_time;

  void (*draw_transparent) (struct state *st, short *src);

  async_load_state *img_loader;

  XShmSegmentInfo shm_info;
};


/* Distribution of drops: many little ones and a few big ones. */
static const double drop_dist[] =
  {0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.2, 0.6};


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


static unsigned long grayscale(struct state *st, unsigned long color);

/*      -------------------------------------------             */


static int
map_color(struct state *st, int grey)
{
  /* Clip it */
  grey = st->ncolors * abs(grey) / (SPLASH/4);
  if (grey > st->ncolors)
    grey = st->ncolors;

  /* Display it */
  return st->ctab[grey];
}


static void
draw_ripple(struct state *st, short *src)
{
  int across, down;
  char *dirty = st->dirty_buffer;

  for (down = 0; down < st->height - 1; down++, src += 1, dirty += 1)
    for (across = 0; across < st->width - 1; across++, src++, dirty++) {
      int v1, v2, v3, v4;
      v1 = (int)*src;
      v2 = (int)*(src + 1);
      v3 = (int)*(src + st->width);
      v4 = (int)*(src + st->width + 1);
      if ((v1 == 0 && v2 == 0 && v3 == 0 && v4 == 0)) {
        if (*dirty > 0)
          (*dirty)--;
      } else
        *dirty = DIRTY;

      if (*dirty > 0) {
        int dx;
        if (st->light > 0) {
          dx = ((v3 - v1) + (v4 - v2)) << st->light; /* light from top */
        } else
          dx = 0;
        XPutPixel(st->buffer_map,(across<<1),  (down<<1),  map_color(st, dx + v1));
        XPutPixel(st->buffer_map,(across<<1)+1,(down<<1),  map_color(st, dx + ((v1 + v2) >> 1)));
        XPutPixel(st->buffer_map,(across<<1),  (down<<1)+1,map_color(st, dx + ((v1 + v3) >> 1)));
        XPutPixel(st->buffer_map,(across<<1)+1,(down<<1)+1,map_color(st, dx + ((v1 + v4) >> 1)));
      }
    }
}


/*      -------------------------------------------             */


/* Uses the horizontal gradient as an offset to create a warp effect  */
static void
draw_transparent_vanilla(struct state *st, short *src)
{
  int across, down, pixel;
  char *dirty = st->dirty_buffer;

  pixel = 0;
  for (down = 0; down < st->height - 2; down++, pixel += 2)
    for (across = 0; across < st->width-2; across++, pixel++) {
      int gradx, grady, gradx1, grady1;
      int x0, x1, x2, y1, y2;

      x0 = src[pixel];
      x1 = src[pixel + 1];
      x2 = src[pixel + 2];
      y1 = src[pixel + st->width];
      y2 = src[pixel + 2*st->width];

      gradx = (x1 - x0);
      grady = (y1 - x0);
      gradx1= (x2 - x1);
      grady1= (y2 - y1);
      gradx1 = 1 + (gradx + gradx1) / 2;
      grady1 = 1 + (grady + grady1) / 2;

      if ((2*across+MIN(gradx,gradx1) < 0) ||
          (2*across+MAX(gradx,gradx1) >= st->bigwidth)) {
        gradx = 0;
        gradx1= 1;
      }
      if ((2*down+MIN(grady,grady1) < 0) ||
          (2*down+MAX(grady,grady1) >= st->bigheight)) {
        grady = 0;
        grady1 = 1;
      }

      if ((gradx == 0 && gradx1 == 1 && grady == 0 && grady1 == 1)) {
        if (dirty[pixel] > 0)
          dirty[pixel]--;
      } else
        dirty[pixel] = DIRTY;

      if (dirty[pixel] > 0) {
        XPutPixel(st->buffer_map, (across<<1),  (down<<1),
                  grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx, (down<<1) + grady)));
        XPutPixel(st->buffer_map, (across<<1)+1,(down<<1),
                  grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx1,(down<<1) + grady)));
        XPutPixel(st->buffer_map, (across<<1),  (down<<1)+1,
                  grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx, (down<<1) + grady1)));
        XPutPixel(st->buffer_map, (across<<1)+1,(down<<1)+1,
                  grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx1,(down<<1) + grady1)));
      }
    }
}


/*      -------------------------------------------             */


static void
set_mask(unsigned long *mask, int *shift)
{
  unsigned long color = *mask;
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
bright(struct state *st, int dx, unsigned long color)
{
  return (cadd(color, dx, st->rmask, st->rshift) |
          cadd(color, dx, st->gmask, st->gshift) |
          cadd(color, dx, st->bmask, st->bshift));
}


static unsigned long
grayscale(struct state *st, unsigned long color)
{
  int red;
  int green;
  int blue;
  int total;
  int gray_r;
  int gray_g;
  int gray_b;

  if (!st->grayscale_p)
    return color;
  if (!st->transparent)
    return color;
  if ((st->rmask == 0) || (st->gmask == 0) || (st->bmask == 0))
    return color;

  red = ((color >> st->rshift) & st->rmask);
  green =  ((color >> st->gshift) & st->gmask);
  blue =  ((color >> st->bshift) & st->bmask);
  total = red * st->gmask * st->bmask + green * st->rmask * st->bmask + blue * st->rmask * st->gmask;

  gray_r = total / (3 * st->gmask * st->bmask);
  if (gray_r < 0)
    gray_r = 0;
  if (gray_r > st->rmask)
    gray_r = st->rmask;
  
  gray_g = total / (3 * st->rmask * st->bmask);
  if (gray_g < 0)
    gray_g = 0;
  if (gray_g > st->gmask)
    gray_g = st->gmask;

  gray_b = total / (3 * st->rmask * st->gmask);
  if (gray_b < 0)
    gray_b = 0;
  if (gray_b > st->bmask)
    gray_b = st->bmask;

  return ((unsigned long)
          ((gray_r << st->rshift) | (gray_g << st->gshift) | (gray_b << st->bshift)));
}


static void
draw_transparent_light(struct state *st, short *src)
{
  int across, down, pixel;
  char *dirty = st->dirty_buffer;

  pixel = 0;
  for (down = 0; down < st->height - 2; down++, pixel += 2)
    for (across = 0; across < st->width-2; across++, pixel++) {
      int gradx, grady, gradx1, grady1;
      int x0, x1, x2, y1, y2;

      x0 = src[pixel];
      x1 = src[pixel + 1];
      x2 = src[pixel + 2];
      y1 = src[pixel + st->width];
      y2 = src[pixel + 2*st->width];

      gradx = (x1 - x0);
      grady = (y1 - x0);
      gradx1= (x2 - x1);
      grady1= (y2 - y1);
      gradx1 = 1 + (gradx + gradx1) / 2;
      grady1 = 1 + (grady + grady1) / 2;

      if ((2*across+MIN(gradx,gradx1) < 0) ||
          (2*across+MAX(gradx,gradx1) >= st->bigwidth)) {
        gradx = 0;
        gradx1= 1;
      }
      if ((2*down+MIN(grady,grady1) < 0) ||
          (2*down+MAX(grady,grady1) >= st->bigheight)) {
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
        if (4-st->light >= 0)
          dx = (grady + (src[pixel+st->width+1]-x1)) >> (4-st->light);
        else
          dx = (grady + (src[pixel+st->width+1]-x1)) << (st->light-4);

        if (dx != 0) {
          XPutPixel(st->buffer_map, (across<<1),  (down<<1),
                    bright(st, dx, grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx, (down<<1) + grady))));
          XPutPixel(st->buffer_map, (across<<1)+1,(down<<1),
                    bright(st, dx, grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx1,(down<<1) + grady))));
          XPutPixel(st->buffer_map, (across<<1),  (down<<1)+1,
                    bright(st, dx, grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx, (down<<1) + grady1))));
          XPutPixel(st->buffer_map, (across<<1)+1,(down<<1)+1,
                    bright(st, dx, grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx1,(down<<1) + grady1))));
        } else {
          /* Could use XCopyArea, but XPutPixel is faster */
          XPutPixel(st->buffer_map, (across<<1),  (down<<1),
                    grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx, (down<<1) + grady)));
          XPutPixel(st->buffer_map, (across<<1)+1,(down<<1),
                    grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx1,(down<<1) + grady)));
          XPutPixel(st->buffer_map, (across<<1),  (down<<1)+1,
                    grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx, (down<<1) + grady1)));
          XPutPixel(st->buffer_map, (across<<1)+1,(down<<1)+1,
                    grayscale(st, XGetPixel(st->orig_map, (across<<1) + gradx1,(down<<1) + grady1)));
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
  char *dirty = st->dirty_buffer;
  unsigned short *buffer, *orig;

  buffer = (unsigned short *) st->buffer_map->data;
  orig = (unsigned short *) st->orig_map->data;

  for (pixel = bigpix = down = 0;
       down < st->height - 2;
       down++, pixel += 2, bigpix += st->bigwidth+4)
    for (across = 0; across < st->width-2; across++, pixel++, bigpix+=2) {
      int gradx, grady, gradx1, grady1;
      int x0, x1, x2, y1, y2;

      x0 = src[pixel];
      x1 = src[pixel + 1];
      x2 = src[pixel + 2];
      y1 = src[pixel + st->width];
      y2 = src[pixel + 2*st->width];

      gradx = (x1 - x0);
      grady = (y1 - x0);
      gradx1= (x2 - x1);
      grady1= (y2 - y1);
      gradx1 = 1 + (gradx + gradx1) / 2;
      grady1 = 1 + (grady + grady1) / 2;

      if ((2*across+MIN(gradx,gradx1) < 0) ||
          (2*across+MAX(gradx,gradx1) >= st->bigwidth)) {
        gradx = 0;
        gradx1= 1;
      }
      if ((2*down+MIN(grady,grady1) < 0) ||
          (2*down+MAX(grady,grady1) >= st->bigheight)) {
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
        if (4-st->light >= 0)
          dx = (grady + (src[pixel+st->width+1]-x1)) >> (4-st->light);
        else
          dx = (grady + (src[pixel+st->width+1]-x1)) << (st->light-4);

        grady *= st->bigwidth;
        grady1*= st->bigwidth;

        if (dx != 0) {
          *dest++ = dobright(dx, *(image + gradx  + grady));
          *dest   = dobright(dx, *(image + gradx1 + grady));
          dest   += st->bigwidth - 1;
          *dest++ = dobright(dx, *(image + gradx  + grady1));
          *dest   = dobright(dx, *(image + gradx1 + grady1));
        } else {
          *dest++ = *(image + gradx  + grady);
          *dest   = *(image + gradx1 + grady);
          dest   += st->bigwidth - 1;
          *dest++ = *(image + gradx  + grady1);
          *dest   = *(image + gradx1 + grady1);
        }
      }
    }
}
#endif


/*      -------------------------------------------             */


static void
setup_X(struct state *st)
{
  XWindowAttributes xgwa;
  int depth;

  XGetWindowAttributes(st->dpy, st->window, &xgwa);
  depth = xgwa.depth;
  st->colormap = xgwa.colormap;
  st->screen = xgwa.screen;
  st->bigwidth = xgwa.width;
  st->bigheight = xgwa.height;
  st->visual = xgwa.visual;


  /* This causes buffer_map to be 1 pixel taller and wider than orig_map,
     which can cause the two XImages to have different bytes-per-line,
     which causes stair-stepping.  So this better not be necessary...
     -jwz, 23-Nov-01
   */
#if 0 /* I'm not entirely sure if I need this */
  if (st->bigwidth % 2)
    st->bigwidth++;
  if (st->bigheight % 2)
    st->bigheight++;
#endif


  st->width = st->bigwidth / 2;
  st->height = st->bigheight / 2;

  if (st->transparent) {
    XGCValues gcv;
    long gcflags;

    gcv.function = GXcopy;
    gcv.subwindow_mode = IncludeInferiors;

    gcflags = GCFunction;
    if (use_subwindow_mode_p(xgwa.screen, st->window))	/* see grabscreen.c */
      gcflags |= GCSubwindowMode;

    st->gc = XCreateGC(st->dpy, st->window, gcflags, &gcv);

    st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                              st->window, 0, 0);
    st->start_time = time ((time_t *) 0);
  } else {
    XGCValues gcv;

    st->gc = XCreateGC(st->dpy, st->window, 0, &gcv);
    st->orig_map = 0;
  }

  if (!st->gc) {
    fprintf(stderr, "XCreateGC failed\n");
    exit(1);
  }

  st->buffer_map = create_xshm_image(st->dpy, xgwa.visual, depth,
                                     ZPixmap, &st->shm_info, st->bigwidth, st->bigheight);
}


static void
DisplayImage(struct state *st)
{
  put_xshm_image(st->dpy, st->window, st->gc, st->buffer_map, 0, 0, 0, 0,
                 st->bigwidth, st->bigheight, &st->shm_info);
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
init_linear_colors(struct state *st)
{
  int i, j, red, green, blue, bred, bgreen, bblue;
  XColor fg, bg;

  if (st->ncolors < 2 || mono_p)
    st->ncolors = 2;
  if (st->ncolors <= 2)
    mono_p = True;

  /* Make it possible to set the color of the ripples,
     Based on work by Raymond Medeiros <ray@stommel.marine.usf.edu> and jwz.
   */
  fg.pixel = get_pixel_resource(st->dpy, st->colormap, "foreground", "Foreground");
  XQueryColor(st->dpy, st->colormap, &fg);
  red = (fg.red >> 8);
  green = (fg.green >> 8);
  blue = (fg.blue >> 8);

  bg.pixel = get_pixel_resource(st->dpy, st->colormap, "background", "Background");
  XQueryColor(st->dpy, st->colormap, &bg);
  bred = (bg.red >> 8);
  bgreen = (bg.green >> 8);
  bblue = (bg.blue >> 8);

  j = 0;
  for (i = 0; i < st->ncolors+1; i++) {
    XColor xcl;
    double a = (double)i / st->ncolors;
    int r = cinterp(a, bred, red);
    int g = cinterp(a, bgreen, green);
    int b = cinterp(a, bblue, blue);

    xcl.red = (unsigned short) ((r << 8) | r);
    xcl.green = (unsigned short) ((g << 8) | g);
    xcl.blue = (unsigned short) ((b << 8) | b);
    xcl.flags = DoRed | DoGreen | DoBlue;

    XAllocColor(st->dpy, st->colormap, &xcl);

    st->ctab[j++] = (int) xcl.pixel;
  }
}


static void
init_oily_colors(struct state *st)
{
  XColor *colors = NULL;

  if (st->ncolors < 2 || mono_p)
    st->ncolors = 2;
  if (st->ncolors <= 2)
    mono_p = True;
  colors = 0;

  if (!mono_p) {
    colors = (XColor *)malloc(sizeof(*colors) * (st->ncolors+1));
    make_smooth_colormap(st->screen, st->visual, st->colormap,
                         colors, &st->ncolors,
                         True, /* allocate */
                         False, /* not writable */
                         True); /* verbose (complain about failure) */
    if (st->ncolors <= 2) {
      if (colors)
        free (colors);
      colors = 0;
      mono_p = True;
    }
  }
  if (!mono_p) {
    int i, j = 0;
    for (i = 0; i < st->ncolors+1; i++) {
      XAllocColor(st->dpy, st->colormap, colors+i);
      st->ctab[j++] = (int) colors[i].pixel;
    }
    free (colors);
  } else {
    st->ncolors = 2;
    st->ctab[1] = get_pixel_resource(st->dpy, st->colormap, "foreground", "Foreground");
    st->ctab[0] = get_pixel_resource(st->dpy, st->colormap, "background", "Background");
  }
}


/*      -------------------------------------------             */


static void
init_cos_tab(struct state *st)
{
  int i;
  for (i = 0; i < TABLE; i++)
    st->cos_tab[i] = cos(i * M_PI/2 / TABLE);
}


/* Shape of drop to add */
static double
sinc(struct state *st, double x)
{
#if 1
  /* cosine hump */
  int i;
  i = (int)(x * TABLE + 0.5);
  if (i >= TABLE) i = (TABLE-1) - (i-(TABLE-1));
  if (i < 0) return 0.;
  return st->cos_tab[i];
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
add_circle_drop(struct state *st, int x, int y, int radius, int dheight)
{
  int r, r2, cx, cy;
  short *buf = (random()&1) ? st->bufferA : st->bufferB;

  r2 = radius * radius;


  for (cy = -radius; cy <= radius; cy++)
    for (cx = -radius; cx <= radius; cx++) {
      int xx = x+cx;
      int yy = y+cy;
      if (xx < 0 || yy < 0 || xx >= st->width || yy >= st->height) {break;}
      r = cx*cx + cy*cy;
      if (r > r2) break;
      buf[xx + yy*st->width] =
        (short)(dheight * sinc(st, (radius > 0) ? sqrt(r)/radius : 0));
    }
}


static void
add_drop(struct state *st, ripple_mode mode, int drop)
{
  int newx, newy, dheight;
  int radius = MIN(st->width, st->height) / 50;
  /* Don't put drops too near the edge of the screen or they get stuck */
  int border = 8;

  switch (mode) {
  default:
  case ripple_drop: {
    int x;

    dheight = 1 + (random() % drop);
    newx = border + (random() % (st->width - 2*border));
    newy = border + (random() % (st->height - 2*border));
    x = newy * st->width + newx;
    st->bufferA[x + 1] = st->bufferA[x] = st->bufferA[x + st->width] = st->bufferA[x + st->width + 1] =
      st->bufferB[x + 1] = st->bufferB[x] = st->bufferB[x + st->width] = st->bufferB[x + st->width + 1] =
      dheight;
  }
  break;
  case ripple_blob: {
    double power;

    int tmp_i, tmp_j;
    power = drop_dist[random() % (sizeof(drop_dist)/sizeof(drop_dist[0]))]; /* clumsy */
    dheight = (int)(drop * (power + 0.01));
    tmp_i = (int)(st->width - 2*border - 2*radius*power);
    tmp_j = (int)(st->height - 2*border - 2*radius*power);
    newx = radius + border + ((tmp_i > 0) ? random() % tmp_i : 0);
    newy = radius + border + ((tmp_j > 0) ? random() % tmp_j : 0);
    add_circle_drop(st, newx, newy, radius, dheight);
  }
  break;
  /* Adding too many boxes too quickly (-box 1) doesn't give the waves time
     to disperse and the waves build up (and overflow) */
  case ripple_box: {
    int x;
    int cx, cy;
    short *buf = (random()&1) ? st->bufferA : st->bufferB;
    int tmp_i, tmp_j;

    radius = (1 + (random() % 5)) * (1 + (random() % 5));
    dheight = drop / 128;
    if (random() & 1) dheight = -dheight;
    tmp_i = st->width - 2*border - 2*radius;
    tmp_j = st->height - 2*border - 2*radius;
    newx = radius + border + ((tmp_i > 0) ? random() % tmp_i : 0);
    newy = radius + border + ((tmp_j > 0) ? random() % tmp_j : 0);
    x = newy * st->width + newx;
    for (cy = -radius; cy <= radius; cy++)
      for (cx = -radius; cx <= radius; cx++)
        buf[x + cx + cy*st->width] = (short)(dheight);
  }
  break;
  case ripple_stir: {
    border += radius;
    newx = border + (int)((st->width-2*border) * (1+cos(3*st->stir_ang)) / 2);
    newy = border + (int)((st->height-2*border) * (1+sin(2*st->stir_ang)) / 2);
    add_circle_drop(st, newx, newy, radius, drop / 10);
    st->stir_ang += 0.02;
    if (st->stir_ang > 12*M_PI) st->stir_ang = 0;
  }
  break;
  }
}


static void
init_ripples(struct state *st, int ndrops, int splash)
{
  int i;

  st->bufferA = (short *)calloc(st->width * st->height, sizeof(*st->bufferA));
  st->bufferB = (short *)calloc(st->width * st->height, sizeof(*st->bufferB));
  st->temp = (short *)calloc(st->width * st->height, sizeof(*st->temp));

  st->dirty_buffer = (char *)calloc(st->width * st->height, sizeof(*st->dirty_buffer));

  for (i = 0; i < ndrops; i++)
    add_drop(st, ripple_blob, splash);

  if (st->transparent) {
    if (st->grayscale_p)
    {
      int across, down;
      for (down = 0; down < st->bigheight; down++)
        for (across = 0; across < st->bigwidth; across++)
          XPutPixel(st->buffer_map, across, down,
                    grayscale(st, XGetPixel(st->orig_map, across, down)));
    }
    else
    {  
    /* There's got to be a better way of doing this  XCopyArea? */
    memcpy(st->buffer_map->data, st->orig_map->data,
           st->bigheight * st->buffer_map->bytes_per_line);
    }
  } else {
    int across, down, color;

    color = map_color(st, 0); /* background colour */
    for (down = 0; down < st->bigheight; down++)
      for (across = 0; across < st->bigwidth; across++)
        XPutPixel(st->buffer_map,across,  down,  color);
  }

  DisplayImage(st);
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
ripple(struct state *st)
{
  int across, down, pixel;
  short *src, *dest;

  if (st->draw_toggle == 0) {
    src = st->bufferA;
    dest = st->bufferB;
    st->draw_toggle = 1;
  } else {
    src = st->bufferB;
    dest = st->bufferA;
    st->draw_toggle = 0;
  }

  switch (st->draw_count) {
  case 0: case 1:
    pixel = 1 * st->width + 1;
    for (down = 1; down < st->height - 1; down++, pixel += 2 * 1)
      for (across = 1; across < st->width - 1; across++, pixel++) {
        st->temp[pixel] =
          (((src[pixel - 1] + src[pixel + 1] +
             src[pixel - st->width] + src[pixel + st->width]) / 2)) - dest[pixel];
      }

    /* Smooth the output */
    pixel = 1 * st->width + 1;
    for (down = 1; down < st->height - 1; down++, pixel += 2 * 1)
      for (across = 1; across < st->width - 1; across++, pixel++) {
        if (st->temp[pixel] != 0) { /* Close enough for government work */
          int damp =
            (st->temp[pixel - 1] + st->temp[pixel + 1] +
             st->temp[pixel - st->width] + st->temp[pixel + st->width] +
             st->temp[pixel - st->width - 1] + st->temp[pixel - st->width + 1] +
             st->temp[pixel + st->width - 1] + st->temp[pixel + st->width + 1] +
             st->temp[pixel]) / 9;
          dest[pixel] = damp - (damp >> st->fluidity);
        } else
          dest[pixel] = 0;
      }
    break;
  case 2: case 3:
    pixel = 1 * st->width + 1;
    for (down = 1; down < st->height - 1; down++, pixel += 2 * 1)
      for (across = 1; across < st->width - 1; across++, pixel++) {
        int damp =
          (((src[pixel - 1] + src[pixel + 1] +
             src[pixel - st->width] + src[pixel + st->width]) / 2)) - dest[pixel];
        dest[pixel] = damp - (damp >> st->fluidity);
      }
    break;
  }
  if (++st->draw_count > 3) st->draw_count = 0;

  if (st->transparent)
    st->draw_transparent(st, dest);
  else
    draw_ripple(st, dest);
}


/*      -------------------------------------------             */

static void *
ripples_init (Display *disp, Window win)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = disp;
  st->window = win;
  st->iterations = 0;
  st->delay = get_integer_resource(disp, "delay", "Integer");
  st->duration = get_integer_resource (st->dpy, "duration", "Seconds");
  st->rate = get_integer_resource(disp, "rate", "Integer");
  st->box = get_integer_resource(disp, "box", "Integer");
  st->oily = get_boolean_resource(disp, "oily", "Boolean");
  st->stir = get_boolean_resource(disp, "stir", "Boolean");
  st->fluidity = get_integer_resource(disp, "fluidity", "Integer");
  st->transparent = get_boolean_resource(disp, "water", "Boolean");
  st->grayscale_p = get_boolean_resource(disp, "grayscale", "Boolean");
  st->light = get_integer_resource(disp, "light", "Integer");

  if (st->delay < 0) st->delay = 0;
  if (st->duration < 1) st->duration = 1;
  if (st->fluidity <= 1) st->fluidity = 1;
  if (st->fluidity > 16) st->fluidity = 16; /* 16 = sizeof(short) */
  if (st->light < 0) st->light = 0;

  init_cos_tab(st);
  setup_X(st);

  st->ncolors = get_integer_resource (disp, "colors", "Colors");
  if (0 == st->ncolors)		/* English spelling? */
    st->ncolors = get_integer_resource (disp, "colours", "Colors");

  if (st->ncolors > sizeof(st->ctab)/sizeof(*st->ctab))
    st->ncolors = sizeof(st->ctab)/sizeof(*st->ctab);

  if (st->oily)
    init_oily_colors(st);
  else
    init_linear_colors(st);

  if (st->transparent && st->light > 0) {
    int maxbits;
    st->draw_transparent = draw_transparent_light;
    visual_rgb_masks (st->screen, st->visual,
                      &st->rmask, &st->gmask, &st->bmask);
    set_mask(&st->rmask, &st->rshift);
    set_mask(&st->gmask, &st->gshift);
    set_mask(&st->bmask, &st->bshift);
    if (st->rmask == 0) st->draw_transparent = draw_transparent_vanilla;

    /* Adjust the shift value "light" when we don't have 8 bits per colour */
    maxbits = MIN(MIN(BITCOUNT(st->rmask), BITCOUNT(st->gmask)), BITCOUNT(st->bmask));
    st->light -= 8-maxbits;
    if (st->light < 0) st->light = 0;
  } else {
    if (st->grayscale_p)
    { 
      visual_rgb_masks (st->screen, st->visual,
                        &st->rmask, &st->gmask, &st->bmask);
      set_mask(&st->rmask, &st->rshift);
      set_mask(&st->gmask, &st->gshift);
      set_mask(&st->bmask, &st->bshift);
    }
    st->draw_transparent = draw_transparent_vanilla;
  }
  
  if (!st->transparent)
    init_ripples(st, 0, -SPLASH); /* Start off without any drops */

  return st;
}

static unsigned long
ripples_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);
      if (! st->img_loader) {  /* just finished */
        XWindowAttributes xgwa;
        XGetWindowAttributes(st->dpy, st->window, &xgwa);
        st->start_time = time ((time_t *) 0);
        st->orig_map = XGetImage (st->dpy, st->window, 0, 0, 
                                  xgwa.width, xgwa.height,
                                  ~0L, ZPixmap);
        init_ripples(st, 0, -SPLASH); /* Start off without any drops */
      }
      return st->delay;
    }

    if (!st->img_loader &&
        st->start_time + st->duration < time ((time_t *) 0)) {
      XWindowAttributes xgwa;
      XGetWindowAttributes(st->dpy, st->window, &xgwa);
      st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                                st->window, 0, 0);
      st->start_time = time ((time_t *) 0);
      return st->delay;
    }

    if (st->rate > 0 && (st->iterations % st->rate) == 0)
      add_drop(st, ripple_blob, -SPLASH);
    if (st->stir)
      add_drop(st, ripple_stir, -SPLASH);
    if (st->box > 0 && (random() % st->box) == 0)
      add_drop(st, ripple_box, -SPLASH);

    ripple(st);
    DisplayImage(st);

    st->iterations++;

    return st->delay;
}


static void
ripples_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
ripples_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->start_time = 0;
      return True;
    }
  return False;
}

static void
ripples_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}

static const char *ripples_defaults[] =
{
  ".background:		black",
  ".foreground:		#FFAF5F",
  "*colors:		200",
  "*dontClearRoot:	True",
  "*delay:		50000",
  "*duration:		120",
  "*rate:	 	5",
  "*box:	 	0",
  "*water: 		True",
  "*oily: 		False",
  "*stir: 		False",
  "*fluidity: 		6",
  "*light: 		4",
  "*grayscale: 		False",
#ifdef HAVE_XSHM_EXTENSION
  "*useSHM: True",
#else
  "*useSHM: False",
#endif
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
  "*rotateImages:   True",
#endif
  0
};

static XrmOptionDescRec ripples_options[] =
{
  { "-colors",	".colors",	XrmoptionSepArg, 0},
  { "-colours",	".colors",	XrmoptionSepArg, 0},
  {"-delay",	".delay",	XrmoptionSepArg, 0},
  {"-duration",	".duration",	XrmoptionSepArg, 0 },
  {"-rate",	".rate",	XrmoptionSepArg, 0},
  {"-box",	".box",		XrmoptionSepArg, 0},
  {"-water",	".water",	XrmoptionNoArg, "True"},
  {"-oily",	".oily",	XrmoptionNoArg, "True"},
  {"-stir",	".stir",	XrmoptionNoArg, "True"},
  {"-fluidity",	".fluidity",	XrmoptionSepArg, 0},
  {"-light",	".light",	XrmoptionSepArg, 0},
  {"-grayscale",	".grayscale",	XrmoptionNoArg, "True"},
  {"-shm",	".useSHM",	XrmoptionNoArg, "True"},
  {"-no-shm",	".useSHM",	XrmoptionNoArg, "False"},
  {0, 0, 0, 0}
};


XSCREENSAVER_MODULE ("Ripples", ripples)
