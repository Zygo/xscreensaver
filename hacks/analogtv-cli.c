/* xanalogtv-cli, Copyright © 2018-2023 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Performs the "Analog TV" transform on an image file, converting it to
 * an MP4.  The MP4 file will have the same dimensions as the largest of
 * the input images.
 *
 *    --duration     Length in seconds of output MP4.
 *    --size WxH     Dimensions of output MP4.  Defaults to dimensions of
 *                   the largest input image.
 *    --slideshow    With multiple files, how many seconds to display
 *                   variants of each file before switching to the next.
 *                   If unspecified, they all go in the mix at once with
 *                   short duration.
 *    --powerup      Do the power-on animation at the beginning, and
 *                   fade to black at the end.
 *    --logo FILE    Small image overlayed onto the colorbars image.
 *    --audio FILE   Add a soundtrack.
 *
 *  Created: 10-Dec-2018 by jwz.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef HAVE_FFMPEG
# error HAVE_FFMPEG is required
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include "screenhackI.h"
#include "resources.h"
#include "visual.h"
#include "yarandom.h"
#include "font-retry.h"
#include "ximage-loader.h"
#include "thread_util.h"
#include "xshm.h"
#include "analogtv.h"
#include "ffmpeg-out.h"

const char *progname;
const char *progclass;
int mono_p = 0;
static Bool verbose_p = 0;

#define MAX_MULTICHAN 2
static int N_CHANNELS=12;
static int MAX_STATIONS=6;

#define POWERUP_DURATION   6  /* Hardcoded in analogtv.c */
#define POWERDOWN_DURATION 1  /* Only used here */

typedef struct chansetting_s {
  analogtv_reception recs[MAX_MULTICHAN];
  double noise_level;
} chansetting;

struct state {
  XImage *output_frame;
  Display *dpy;
  Window window;
  analogtv *tv;
  analogtv_font ugly_font;

  int n_stations;
  analogtv_input **stations;
  Bool image_loading_p;
  XImage *logo, *logo_mask;

  int curinputi;
  chansetting *chansettings;
  chansetting *cs;
};

static struct state global_state;


/* Since this program does not connect to an X server, or in fact link
   with Xlib, we need stubs for the few X11 routines that analogtv.c calls.
   Most are unused. It seems like I am forever implementing subsets of X11.
 */

Status
XAllocColor (Display *dpy, Colormap cmap, XColor *c)
{
  abort();
}

int
XClearArea (Display *dpy, Window win, int x, int y,
            unsigned int w, unsigned int h, Bool exp)
{
  return 0;
}

int
XClearWindow (Display *dpy, Window window)
{
  return 0;
}

GC
XCreateGC(Display *dpy, Drawable d, unsigned long mask, XGCValues *gcv)
{
  return 0;
}

int screen_number (Screen *screen) { return 0; }

#undef DisplayOfScreen
#define DisplayOfScreen(s) 0

XImage *
XCreateImage (Display *dpy, Visual *v, unsigned int depth,
              int format, int offset, char *data,
              unsigned int width, unsigned int height,
              int bitmap_pad, int bytes_per_line)
{
  XImage *ximage = (XImage *) calloc (1, sizeof(*ximage));
  unsigned long r, g, b;

  if (depth == 0) depth = 32;

  ximage->width = width;
  ximage->height = height;
  ximage->format = format;
  ximage->data = data;
  ximage->bitmap_unit = 8;
  ximage->byte_order = LSBFirst;
  ximage->bitmap_bit_order = ximage->byte_order;
  ximage->bitmap_pad = bitmap_pad;
  ximage->depth = depth;
  visual_rgb_masks (0, v, &r, &g, &b);
  ximage->red_mask   = (depth == 1 ? 0 : r);
  ximage->green_mask = (depth == 1 ? 0 : g);
  ximage->blue_mask  = (depth == 1 ? 0 : b);
  ximage->bits_per_pixel = (depth == 1 ? 1 : visual_pixmap_depth (0, v));
  ximage->bytes_per_line = bytes_per_line;

  XInitImage (ximage);
  if (! ximage->f.put_pixel) abort();
  return ximage;
}

Pixmap
XCreatePixmap (Display *dpy, Drawable d, unsigned int width,
               unsigned int height, unsigned int depth)
{
  abort();
}

Pixmap
XCreatePixmapFromBitmapData (Display *dpy, Drawable d, char *data,
                             unsigned int w, unsigned int h,
                             unsigned long fg, unsigned long bg,
                             unsigned int depth)
{
  abort();
}

int
XDrawString (Display *dpy, Drawable d, GC gc, int x, int y, const char *s,
             int len)
{
  abort();
}

int
XFillRectangle (Display *dpy, Drawable d, GC gc, int x, int y, 
                unsigned int width, unsigned int height)
{
  abort();
}

int
XFreeColors (Display *dpy, Colormap cmap, unsigned long *px, int n,
             unsigned long planes)
{
  abort();
}

int
XFreeGC (Display *dpy, GC gc)
{
  abort();
}

int
XFreePixmap (Display *dpy, Pixmap p)
{
  abort();
}

XImage *
XGetImage (Display *dpy, Drawable d, int x, int y,
           unsigned int w, unsigned int h,
           unsigned long pm, int fmt)
{
  abort();
}

Status
XGetWindowAttributes (Display *dpy, Window w, XWindowAttributes *xgwa)
{
  struct state *st = &global_state;
  memset (xgwa, 0, sizeof(*xgwa));
  xgwa->width = st->output_frame->width;
  xgwa->height = st->output_frame->height;
  return True;
}

static uint32_t *image_ptr(XImage *image, unsigned int x, unsigned int y)
{
  return (uint32_t *) (image->data + image->bytes_per_line * y) + x;
}

int
XPutImage (Display *dpy, Drawable d, GC gc, XImage *image, 
           int src_x, int src_y, int dest_x, int dest_y,
           unsigned int w, unsigned int h)
{
  struct state *st = &global_state;
  XImage *out = st->output_frame;
  int y;

  if (src_x < 0)
    {
      w += src_x;
      dest_x -= src_x;
      src_x = 0;
    }
  if (dest_x < 0)
    {
      w += dest_x;
      src_x -= dest_x;
      dest_x = 0;
    }
  if (src_x + w > image->width)
    w = image->width - src_x;
  if (dest_x + w > out->width)
    w = out->width - dest_x;

  for (y = 0; y < h; y++) {
    int iy = src_y + y;
    int oy = dest_y + y;
    if (iy >= 0 &&
        oy >= 0 &&
        iy < image->height &&
        oy < out->height)
      memcpy (image_ptr (out, dest_x, oy),
              image_ptr (image, src_x, iy),
              4 * w);
  }
  return 0;
}

int
XQueryColor (Display *dpy, Colormap cmap, XColor *color)
{
  uint16_t r = (color->pixel & 0x00FF0000L) >> 16;
  uint16_t g = (color->pixel & 0x0000FF00L) >> 8;
  uint16_t b = (color->pixel & 0x000000FFL);
  color->red   = r | (r<<8);
  color->green = g | (g<<8);
  color->blue  = b | (b<<8);
  color->flags = DoRed|DoGreen|DoBlue;
  return 0;
}

int
XQueryColors (Display *dpy, Colormap cmap, XColor *c, int n)
{
  int i;
  for (i = 0; i < n; i++)
    XQueryColor (dpy, cmap, &c[i]);
  return 0;
}

int
XSetForeground (Display *dpy, GC gc, unsigned long fg)
{
  abort();
}

int
XSetWindowBackground (Display *dpy, Window win, unsigned long bg)
{
  return 0;
}

XImage *
create_xshm_image (Display *dpy, Visual *visual,
		   unsigned int depth,
		   int format, XShmSegmentInfo *shm_info,
		   unsigned int width, unsigned int height)
{
# undef BitmapPad
# define BitmapPad(dpy) 8
  XImage *image = XCreateImage (dpy, visual, depth, format, 0, NULL,
                                width, height, BitmapPad(dpy), 0);
  int error = thread_malloc ((void **)&image->data, dpy,
                             image->height * image->bytes_per_line);
  if (error) {
    XDestroyImage (image);
    image = NULL;
  } else {
    memset (image->data, 0, image->height * image->bytes_per_line);
  }

  return image;
}

void
destroy_xshm_image (Display *dpy, XImage *image, XShmSegmentInfo *shm_info)
{
  thread_free (image->data);
  image->data = NULL;
  XDestroyImage (image);
}

Bool
get_boolean_resource (Display *dpy, char *name, char *class)
{
  if (!strcmp(name, "useThreads")) return True;
  abort();
}

static int darkp = 0;
double
get_float_resource (Display *dpy, char *name, char *class)
{
  if (!strcmp(name, "TVTint")) return 5;		/* default 5   */
  if (!strcmp(name, "TVColor")) return 70;		/* default 70  */
  if (!strcmp(name, "TVBrightness"))
    return (darkp ? -15 : 2);				/* default 2   */
  if (!strcmp(name, "TVContrast")) return 150;		/* default 150 */
  abort();
}

int
get_integer_resource (Display *dpy, char *name, char *class)
{
  if (!strcmp(name, "use_cmap")) return 0;
  abort();
}

unsigned int
get_pixel_resource (Display *dpy, Colormap cmap, char *name, char *class)
{
  if (!strcmp(name, "background")) return 0;
  abort();
}

Bool
put_xshm_image (Display *dpy, Drawable d, GC gc, XImage *image,
                int src_x, int src_y, int dest_x, int dest_y,
                unsigned int width, unsigned int height,
                XShmSegmentInfo *shm_info)
{
  return XPutImage (dpy, d, gc, image, src_x, src_y, dest_x, dest_y,
                    width, height);
}

int
visual_class (Screen *s, Visual *v)
{
  return TrueColor;
}

int
visual_depth (Screen *s, Visual *v)
{
  return 32;
}

int
visual_pixmap_depth (Screen *s, Visual *v)
{
  return 32;
}

void
visual_rgb_masks (Screen *screen, Visual *visual,
                  unsigned long *red_mask,
                  unsigned long *green_mask,
                  unsigned long *blue_mask)
{
  *red_mask   = 0x00FF0000L;
  *green_mask = 0x0000FF00L;
  *blue_mask  = 0x000000FFL;
}


static void
update_smpte_colorbars(analogtv_input *input)
{
  struct state *st = (struct state *) input->client_data;
  int col;
  int black_ntsc[4];

  /* 
     SMPTE is the society of motion picture and television engineers, and
     these are the standard color bars in the US. Following the partial spec
     at http://broadcastengineering.com/ar/broadcasting_inside_color_bars/
     These are luma, chroma, and phase numbers for each of the 7 bars.
  */
  double top_cb_table[7][3]={
    {75, 0, 0.0},    /* gray */
    {69, 31, 167.0}, /* yellow */
    {56, 44, 283.5}, /* cyan */
    {48, 41, 240.5}, /* green */
    {36, 41, 60.5},  /* magenta */
    {28, 44, 103.5}, /* red */
    {15, 31, 347.0}  /* blue */
  };
  double mid_cb_table[7][3]={
    {15, 31, 347.0}, /* blue */
    {7, 0, 0},       /* black */
    {36, 41, 60.5},  /* magenta */
    {7, 0, 0},       /* black */
    {56, 44, 283.5}, /* cyan */
    {7, 0, 0},       /* black */
    {75, 0, 0.0}     /* gray */
  };

  analogtv_lcp_to_ntsc(0.0, 0.0, 0.0, black_ntsc);

  analogtv_setup_sync(input, 1, 0);

  for (col=0; col<7; col++) {
    analogtv_draw_solid_rel_lcp (input,
                                 col*(1.0/7.0),
                                 (col+1)*(1.0/7.0),
                                 0.00, 0.68, 
                                 top_cb_table[col][0], 
                                 top_cb_table[col][1],
                                 top_cb_table[col][2]);
    
    analogtv_draw_solid_rel_lcp(input,
                                col*(1.0/7.0),
                                (col+1)*(1.0/7.0),
                                0.68, 0.75, 
                                mid_cb_table[col][0], 
                                mid_cb_table[col][1],
                                mid_cb_table[col][2]);
  }

  analogtv_draw_solid_rel_lcp(input, 0.0, 1.0/6.0,
                              0.75, 1.00, 7, 40, 303);   /* -I */
  analogtv_draw_solid_rel_lcp(input, 1.0/6.0, 2.0/6.0,
                              0.75, 1.00, 100, 0, 0);    /* white */
  analogtv_draw_solid_rel_lcp(input, 2.0/6.0, 3.0/6.0,
                              0.75, 1.00, 7, 40, 33);    /* +Q */
  analogtv_draw_solid_rel_lcp(input, 3.0/6.0, 4.0/6.0,
                              0.75, 1.00, 7, 0, 0);      /* black */
  analogtv_draw_solid_rel_lcp(input, 12.0/18.0, 13.0/18.0,
                              0.75, 1.00, 3, 0, 0);      /* black -4 */
  analogtv_draw_solid_rel_lcp(input, 13.0/18.0, 14.0/18.0,
                              0.75, 1.00, 7, 0, 0);      /* black */
  analogtv_draw_solid_rel_lcp(input, 14.0/18.0, 15.0/18.0,
                              0.75, 1.00, 11, 0, 0);     /* black +4 */
  analogtv_draw_solid_rel_lcp(input, 5.0/6.0, 6.0/6.0,
                              0.75, 1.00, 7, 0, 0);      /* black */
  if (st->logo)
    {
      double aspect = (double)
        st->output_frame->width / st->output_frame->height;
      double scale = (aspect > 1 ? 0.35 : 0.6);
      int w2 = st->tv->xgwa.width  * scale;
      int h2 = st->tv->xgwa.height * scale * aspect;
      analogtv_load_ximage (st->tv, input, st->logo, st->logo_mask,
                            (st->tv->xgwa.width - w2) / 2,
                            st->tv->xgwa.height * 0.20,
                            w2, h2);
    }

  input->next_update_time += 1.0;
}


static void
flip_ximage (XImage *ximage)
{
  char *data2, *in, *out;
  int y;

  if (!ximage) return;
  data2 = malloc (ximage->bytes_per_line * ximage->height);
  if (!data2) abort();
  in = ximage->data;
  out = data2 + ximage->bytes_per_line * (ximage->height - 1);
  for (y = 0; y < ximage->height; y++)
    {
      memcpy (out, in, ximage->bytes_per_line);
      in  += ximage->bytes_per_line;
      out -= ximage->bytes_per_line;
    }
  free (ximage->data);
  ximage->data = data2;
}


/* Scales an XImage, modifying it in place.
   This doesn't do dithering or smoothing, so it might have artifacts.
   If out of memory, returns False, and the XImage will have been
   destroyed and freed.
 */
static Bool
scale_ximage (Screen *screen, Visual *visual,
              XImage *ximage, int new_width, int new_height)
{
  Display *dpy = DisplayOfScreen (screen);
  int depth = visual_depth (screen, visual);
  int x, y;
  double xscale, yscale;

  XImage *ximage2 = XCreateImage (dpy, visual, depth,
                                  ZPixmap, 0, 0,
                                  new_width, new_height, 8, 0);
  ximage2->data = (char *) calloc (ximage2->height, ximage2->bytes_per_line);

  if (!ximage2->data)
    {
      fprintf (stderr, "%s: out of memory scaling %dx%d image to %dx%d\n",
               progname,
               ximage->width, ximage->height,
               ximage2->width, ximage2->height);
      if (ximage->data) free (ximage->data);
      if (ximage2->data) free (ximage2->data);
      ximage->data = 0;
      ximage2->data = 0;
      XDestroyImage (ximage);
      XDestroyImage (ximage2);
      return False;
    }

  /* Brute force scaling... */
  xscale = (double) ximage->width  / ximage2->width;
  yscale = (double) ximage->height / ximage2->height;
  for (y = 0; y < ximage2->height; y++)
    for (x = 0; x < ximage2->width; x++)
      XPutPixel (ximage2, x, y, XGetPixel (ximage, x * xscale, y * yscale));

  free (ximage->data);
  ximage->data = 0;

  (*ximage) = (*ximage2);

  ximage2->data = 0;
  XDestroyImage (ximage2);

  return True;
}


static void
analogtv_convert (const char **infiles, const char *outfile,
                  const char *audiofile, const char *logofile,
                  int output_w, int output_h,
                  int duration, int slideshow, Bool powerp)
{
  unsigned long start_time = time((time_t *)0);
  struct state *st = &global_state;
  Display *dpy = 0;
  Window window = 0;
  Screen *screen = 0;
  Visual *visual = 0;
  int i;
  int nfiles;
  unsigned long curticks = 0, curticks_sub = 0;
  time_t lastlog = time((time_t *)0);
  int frames_left = 0;
  int channel_changes = 0;
  int fps = 30;
  XImage **ximages;
  XImage *base_image = 0;
  int *stats;
  ffmpeg_out_state *ffst = 0;

  /* Load all of the input images.
   */
  stats = (int *) calloc(N_CHANNELS, sizeof(*stats));
  for (nfiles = 0; infiles[nfiles]; nfiles++)
    ;
  ximages = calloc (nfiles, sizeof(*ximages));

  {
    int maxw = 0, maxh = 0;
    for (i = 0; i < nfiles; i++)
      {
        XImage *ximage = file_to_ximage (0, 0, infiles[i]);
        ximages[i] = ximage;
        if (verbose_p > 1)
          fprintf (stderr, "%s: loaded %s %dx%d\n", progname, infiles[i],
                   ximage->width, ximage->height);
        flip_ximage (ximage);
        if (ximage->width  > maxw) maxw = ximage->width;
        if (ximage->height > maxh) maxh = ximage->height;
      }

    if (!output_w || !output_h) {
      output_w = maxw;
      output_h = maxh;
    }
  }

  output_w &= ~1;  /* can't be odd */
  output_h &= ~1;

  /* Scale all of the input images to the size of the largest one, or frame.
   */
  for (i = 0; i < nfiles; i++)
    {
      XImage *ximage = ximages[i];
      if (ximage->width != output_w || ximage->height != output_h)
        {
          double r1 = (double) output_w / output_h;
          double r2 = (double) ximage->width / ximage->height;
          int w2, h2;
          if (r1 > r2)
            {
              w2 = output_h * r2;
              h2 = output_h;
            }
          else
            {
              w2 = output_w;
              h2 = output_w / r2;
            }
          if (! scale_ximage (screen, visual, ximage, w2, h2))
            abort();
        }
    }

  memset (st, 0, sizeof(*st));
  st->dpy = dpy;
  st->window = window;

  st->output_frame = XCreateImage (dpy, 0, ximages[0]->depth,
                                   ximages[0]->format, 0, NULL,
                                   output_w, output_h,
                                   ximages[0]->bitmap_pad, 0);
  st->output_frame->data = (char *)
    calloc (st->output_frame->height, st->output_frame->bytes_per_line);

  if (logofile) {
    int x, y;
    st->logo = file_to_ximage (0, 0, logofile);
    if (verbose_p)
      fprintf (stderr, "%s: loaded %s %dx%d\n", 
               progname, logofile, st->logo->width, st->logo->height);
    flip_ximage (st->logo);
    /* Pull the alpha out of the logo and make a separate mask ximage. */
    st->logo_mask = XCreateImage (dpy, 0, st->logo->depth, st->logo->format, 0,
                                  NULL, st->logo->width, st->logo->height,
                                  st->logo->bitmap_pad, 0);
    st->logo_mask->data = (char *)
    calloc (st->logo_mask->height, st->logo_mask->bytes_per_line);

    for (y = 0; y < st->logo->height; y++)
      for (x = 0; x < st->logo->width; x++) {
        unsigned long p = XGetPixel (st->logo, x, y);
        uint8_t a =                     (p & 0xFF000000L) >> 24;
        XPutPixel (st->logo,      x, y, (p & 0x00FFFFFFL));
        XPutPixel (st->logo_mask, x, y, (a ? 0x00FFFFFFL : 0));
      }
  }

  st->tv=analogtv_allocate(dpy, window);

  st->stations = (analogtv_input **)
    calloc (MAX_STATIONS, sizeof(*st->stations));
  while (st->n_stations < MAX_STATIONS) {
    analogtv_input *input=analogtv_input_allocate();
    st->stations[st->n_stations++]=input;
    input->client_data = st;
  }

  analogtv_set_defaults(st->tv, "");
  st->tv->need_clear=1;

  if (random()%4==0) {
    st->tv->tint_control += pow(frand(2.0)-1.0, 7) * 180.0;
  }
  if (1) {
    st->tv->color_control += frand(0.3) * RANDSIGN();
  }
  if (darkp) {
    if (random()%4==0) {
      st->tv->brightness_control += frand(0.15);
    }
    if (random()%4==0) {
      st->tv->contrast_control += frand(0.2) * RANDSIGN();
    }
  }

  st->chansettings = calloc (N_CHANNELS, sizeof (*st->chansettings));
  for (i = 0; i < N_CHANNELS; i++) {
    st->chansettings[i].noise_level = 0.06;
    {
      int last_station=42;
      int stati;
      for (stati=0; stati<MAX_MULTICHAN; stati++) {
        analogtv_reception *rec=&st->chansettings[i].recs[stati];
        int station;
        while (1) {
          station=random()%st->n_stations;
          if (station!=last_station) break;
          if ((random()%10)==0) break;
        }
        last_station=station;
        rec->input = st->stations[station];
        rec->level = pow(frand(1.0), 3.0) * 2.0 + 0.05;
        rec->ofs=random()%ANALOGTV_SIGNAL_LEN;
        if (random()%3) {
          rec->multipath = frand(1.0);
        } else {
          rec->multipath=0.0;
        }
        if (stati) {
          /* We only set a frequency error for ghosting stations,
             because it doesn't matter otherwise */
          rec->freqerr = (frand(2.0)-1.0) * 3.0;
        }

        if (rec->level > 0.3) break;
        if (random()%4) break;
      }
    }
  }

  st->curinputi=0;
  st->cs = &st->chansettings[st->curinputi];

  ffst = ffmpeg_out_init (outfile, audiofile,
                          st->output_frame->width, st->output_frame->height,
                          4, True);

 INIT_CHANNELS:

  curticks_sub = 0;
  channel_changes = 0;
  st->curinputi = 0;
  st->tv->powerup = 0.0;

  if (slideshow)
    /* First channel (initial unadulterated image) stays for this long */
    frames_left = fps * (2 + frand(1.5));

  if (slideshow) {
    /* Pick one of the input images and fill all channels with variants
       of it.
     */
    int n = random() % nfiles;
    XImage *ximage = ximages[n];
    base_image = ximage;
    if (verbose_p > 1)
      fprintf (stderr, "%s: initializing for %s %dx%d in %d channels\n", 
               progname, infiles[n], ximage->width, ximage->height,
               MAX_STATIONS);

    for (i = 0; i < MAX_STATIONS; i++) {
      analogtv_input *input = st->stations[i];
      int w = ximage->width  * 0.815;  /* underscan */
      int h = ximage->height * 0.970;
      int x = (output_w - w) / 2;
      int y = (output_h - h) / 2;

      if (i == 1) {   /* station 0 is the unadulterated image.
                         station 1 is colorbars. */
        input->updater = update_smpte_colorbars;
      }

      analogtv_setup_sync (input, 1, (random()%20)==0);
      analogtv_load_ximage (st->tv, input, ximage, 0, x, y, w, h);
    }
  } else {
    /* Fill all channels with images */
    if (verbose_p > 1)
      fprintf (stderr, "%s: initializing %d files in %d channels\n",
               progname, nfiles, MAX_STATIONS);

    for (i = 0; i < MAX_STATIONS; i++) {
      XImage *ximage = ximages[i % nfiles];
      analogtv_input *input = st->stations[i];
      int w = ximage->width  * 0.815;  /* underscan */
      int h = ximage->height * 0.970;
      int x = (output_w - w) / 2;
      int y = (output_h - h) / 2;

      if (! (random() % 8))  /* Some stations are colorbars */
        input->updater = update_smpte_colorbars;

      analogtv_setup_sync (input, 1, (random()%20)==0);
      analogtv_load_ximage (st->tv, input, ximage, 0, x, y, w, h);
    }
  }

  /* This is xanalogtv_draw()
   */
  while (1) {
    const analogtv_reception *recs[MAX_MULTICHAN];
    unsigned rec_count = 0;
    double curtime = curticks * 0.001;
    double curtime_sub = curticks_sub * 0.001;

    frames_left--;
    if (frames_left <= 0 &&
        (!powerp || curticks > POWERUP_DURATION*1000)) {

      channel_changes++;

      if (slideshow && channel_changes == 1) {
        /* Second channel has short duration, 0.25 to 0.75 sec. */
        frames_left = fps * (0.25 + frand(0.5));
      } else if (slideshow) {
        /* 0.5 - 2.0 sec (was 0.5 - 3.0 sec) */
        frames_left = fps * (0.5 + frand(1.5));
      } else {
        /* 1 - 7 sec */
        frames_left = fps * (1 + frand(6));
      }

      if (slideshow && channel_changes == 2) {
        /* Always use the unadulterated image for the third channel:
           So the effect is, plain, brief blip, plain, then random. */
        st->curinputi = 0;
        frames_left += fps * (0.1 + frand(0.5));

      } else if (slideshow && st->curinputi != 0 && ((random() % 100) < 75)) {
        /* Use the unadulterated image 75% of the time (was 33%) */
        st->curinputi = 0;
      } else {
        /* Otherwise random */
        int prev = st->curinputi;
      AGAIN:
        st->curinputi = 1 + (random() % (N_CHANNELS - 1));

        /* In slideshow mode, always alternate to the unadulterated image:
           no two noisy images in a row, always intersperse clean. */
        if (slideshow && prev != 0)
          st->curinputi = 0;

        /* In slideshow mode, do colorbars-only a bit less often. */
        if (slideshow && st->curinputi == 1 && !(random() % 3))
          goto AGAIN;
      }

      stats[st->curinputi]++;
      st->cs = &st->chansettings[st->curinputi];
      /* Set channel change noise flag */
      st->tv->channel_change_cycles=200000;

      if (verbose_p > 1)
        fprintf (stderr, "%s: %5.1f sec: channel %d\n",
                 progname, curticks/1000.0, st->curinputi);

      /* Turn the knobs every now and then */
      if (! (random() % 5)) {
        if (random()%4==0) {
          st->tv->tint_control += pow(frand(2.0)-1.0, 7) * 180.0 * RANDSIGN();
        }
        if (1) {
          st->tv->color_control += frand(0.3) * RANDSIGN();
        }
        if (darkp) {
          if (random()%4==0) {
            st->tv->brightness_control += frand(0.15);
          }
          if (random()%4==0) {
            st->tv->contrast_control += frand(0.2) * RANDSIGN();
          }
        }
      }
    }

    for (i=0; i<MAX_MULTICHAN; i++) {
      analogtv_reception *rec = &st->cs->recs[i];
      analogtv_input *inp=rec->input;
      if (!inp) continue;

      if (inp->updater) {
        inp->next_update_time = curtime;
        (inp->updater)(inp);
      }
      rec->ofs += rec->freqerr;
    }

    st->tv->powerup=(powerp ? curtime : 9999);

    for (i=0; i<MAX_MULTICHAN; i++) {
      /* Noisy image */
      analogtv_reception *rec = &st->cs->recs[i];
      if (rec->input) {
        analogtv_reception_update(rec);
        recs[rec_count] = rec;
        ++rec_count;
      }

      analogtv_draw (st->tv, st->cs->noise_level, recs, rec_count);

      if (slideshow && st->curinputi == 0 &&
          (!powerp || curticks > POWERUP_DURATION*1000)) {
        /* Unadulterated image centered on top of border of static */
        XImage *ximage = base_image;
        int w = ximage->width;
        int h = ximage->height;
        int x = (output_w - w) / 2;
        int y = (output_h - h) / 2;
        XPutImage (dpy, 0, 0, ximage, 0, 0, x, y, w, h);
      }
    }

    ffmpeg_out_add_frame (ffst, st->output_frame);

    if (powerp &&
        curticks > (duration*1000) - (POWERDOWN_DURATION*1000)) {
      /* Fade out, as there is no power-down animation. */
      double r = ((duration*1000 - curticks) /
                  (double) (POWERDOWN_DURATION*1000));
      static double ob = 9999;
      double min = -1.5;  /* Usable range is something like -0.75 to 1.0 */
      if (ob == 9999) ob = st->tv->brightness_control;  /* Terrible */
      st->tv->brightness_control = min + (ob - min) * r;
    }

    if (curtime >= duration) break;

    if (slideshow && curtime_sub >= slideshow)
      goto INIT_CHANNELS;

    curticks     += 1000/fps;
    curticks_sub += 1000/fps;

    if (verbose_p) {
      unsigned long now = time((time_t *)0);
      if (now > (verbose_p == 1 ? lastlog : lastlog + 10)) {
        unsigned long elapsed = now - start_time;
        double ratio = curtime / (double) duration;
        int remaining = (ratio ? (elapsed / ratio) - elapsed : 0);
        int pct = 100 * ratio;
        int cols = 47;
        char dots[80];
        int ii;
        for (ii = 0; ii < cols * ratio; ii++)
          dots[ii] = '.';
        dots[ii] = 0;
        fprintf (stderr, "%s%s: %s %2d%%, %d:%02d:%02d ETA%s",
                 (verbose_p == 1 ? "\r" : ""),
                 progname,
                 dots, pct, 
                 (remaining/60/60),
                 (remaining/60)%60,
                 remaining%60,
                 (verbose_p == 1 ? "" : "\n"));
        lastlog = now;
      }
    }
  }

  if (verbose_p == 1) fprintf(stderr, "\n");

  if (verbose_p > 1) {
    if (channel_changes == 0) channel_changes++;
    fprintf(stderr, "%s: channels shown: %d\n", progname, channel_changes);
    for (i = 0; i < N_CHANNELS; i++)
      fprintf(stderr, "%s:   %2d: %3d%%\n", progname,
              i+1, stats[i] * 100 / channel_changes);
  }

  free (stats);
  ffmpeg_out_close (ffst);
}


static void
usage(const char *err)
{
  if (err) fprintf (stderr, "%s: %s unknown\n", progname, err);
  fprintf (stderr, "usage: %s [--verbose] [--duration secs] [--slideshow secs]"
           " [--audio mp3-file] [--powerup] [--size WxH]"
           " infile.png ... outfile.mp4\n",
           progname);
  exit (1);
}

int
main (int argc, char **argv)
{
  int i;
  const char *infiles[1000];
  const char *outfile = 0;
  int duration = 30;
  Bool powerp = False;
  char *audio = 0;
  char *logo = 0;
  int w = 0, h = 0;
  int nfiles = 0;
  int slideshow = 0;

  char *s = strrchr (argv[0], '/');
  progname = s ? s+1 : argv[0];
  progclass = progname;

  memset (infiles, 0, sizeof(infiles));

  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;
       if (!strcmp(argv[i], "-v") ||
           !strcmp(argv[i], "-verbose"))
        verbose_p++;
       else if (!strcmp(argv[i], "-vv")) verbose_p += 2;
       else if (!strcmp(argv[i], "-vvv")) verbose_p += 3;
       else if (!strcmp(argv[i], "-vvvv")) verbose_p += 4;
       else if (!strcmp(argv[i], "-vvvvv")) verbose_p += 5;
       else if (!strcmp(argv[i], "-duration") && argv[i+1])
         {
           char dummy;
           i++;
           if (1 != sscanf (argv[i], " %d %c", &duration, &dummy))
             usage(argv[i]);
         }
       else if (!strcmp(argv[i], "-slideshow") && argv[i+1])
         {
           char dummy;
           i++;
           if (1 != sscanf (argv[i], " %d %c", &slideshow, &dummy))
             usage(argv[i]);
         }
       else if (!strcmp(argv[i], "-audio") && argv[i+1])
         audio = argv[++i];
       else if (!strcmp(argv[i], "-size") && argv[i+1])
         {
           char dummy;
           i++;
           if (2 != sscanf (argv[i], " %d x %d %c", &w, &h, &dummy))
             usage(argv[i]);
         }
       else if (!strcmp(argv[i], "-logo") && argv[i+1])
         logo = argv[++i];
       else if (!strcmp(argv[i], "-powerup") ||
                !strcmp(argv[i], "-power"))
         powerp = True;
       else if (!strcmp(argv[i], "-no-powerup") ||
                !strcmp(argv[i], "-no-power"))
         powerp = False;
      else if (argv[i][0] == '-')
        usage(argv[i]);
      else if (nfiles >= countof(infiles)-1)
        usage("too many files");
      else
        infiles[nfiles++] = argv[i];
    }

  if (nfiles < 2)
    usage("");

  outfile = infiles[nfiles-1];
  infiles[--nfiles] = 0;

  if (nfiles == 1)
    slideshow = duration;

  /* stations should be a multiple of files, but >= 6.
     channels should be double that. */
  MAX_STATIONS = 6;
  if (! slideshow) {
    MAX_STATIONS = 0;
    while (MAX_STATIONS < 6)
      MAX_STATIONS += nfiles;
    MAX_STATIONS *= 2;
  }
  N_CHANNELS = MAX_STATIONS * 2;

  darkp = (nfiles == 1);

# undef ya_rand_init
  ya_rand_init (0);
  analogtv_convert (infiles, outfile, audio, logo,
                    w, h, duration, slideshow, powerp);
  exit (0);
}
