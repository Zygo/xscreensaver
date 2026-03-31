/* xscreensaver, Copyright © 1998-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Apple ][ CRT simulator, by Trevor Blackwell <tlb@tlb.org>
 * with additional work by Jamie Zawinski <jwz@jwz.org>
 * Pty and vt100 emulation by Fredrik Tolf <fredrik@dolda2000.com>
 */

#include "screenhack.h"
#include "apple2.h"
#include "textclient.h"
#include "ansi-tty.h"
#include "utf8wc.h"

#include <math.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define SCREEN_COLS 40
#define SCREEN_ROWS 24


/* Given a bitmask, returns the position and width of the field.
 */
static void
decode_mask (unsigned int mask, unsigned int *pos_ret, unsigned int *size_ret)
{
  int i;
  for (i = 0; i < 32; i++)
    if (mask & (1L << i))
      {
        int j = 0;
        *pos_ret = i;
        for (; i < 32; i++, j++)
          if (! (mask & (1L << i)))
            break;
        *size_ret = j;
        return;
      }
}


/* Given a value and a field-width, expands the field to fill out 8 bits.
 */
static unsigned char
spread_bits (unsigned char value, unsigned char width)
{
  switch (width)
    {
    case 8: return value;
    case 7: return (value << 1) | (value >> 6);
    case 6: return (value << 2) | (value >> 4);
    case 5: return (value << 3) | (value >> 2);
    case 4: return (value << 4) | (value);
    case 3: return (value << 5) | (value << 2) | (value >> 2);
    case 2: return (value << 6) | (value << 4) | (value);
    default: abort(); break;
    }
}


/* Convert an XImage (of any size/depth/visual) to a 32bpp RGB array.
   Scales it (without dithering) to WxH.
 */
static void
scale_image (Display *dpy, Window window, XImage *in,
             int fromx, int fromy, int fromw, int fromh,
             unsigned int *out, int w, int h)
{
  float scale;
  int x, y, i;
  unsigned int rpos=0, gpos=0, bpos=0; /* bitfield positions */
  unsigned int rsiz=0, gsiz=0, bsiz=0;
  unsigned long rmsk=0, gmsk=0, bmsk=0;
  unsigned char spread_map[3][256];
  XWindowAttributes xgwa;
  XColor *colors = 0;

  if (fromx + fromw > in->width ||
      fromy + fromh > in->height)
    abort();

  XGetWindowAttributes (dpy, window, &xgwa);

  /* Compute the field offsets for RGB decoding in the XImage,
     when in TrueColor mode.  Otherwise we use the colormap.
   */
  if (visual_class (xgwa.screen, xgwa.visual) == PseudoColor ||
      visual_class (xgwa.screen, xgwa.visual) == GrayScale)
    {
      int ncolors = visual_cells (xgwa.screen, xgwa.visual);
      colors = (XColor *) calloc (sizeof (*colors), ncolors+1);
      for (i = 0; i < ncolors; i++)
        colors[i].pixel = i;
      XQueryColors (dpy, xgwa.colormap, colors, ncolors);
    }
  else
    {
      visual_rgb_masks (xgwa.screen, xgwa.visual, &rmsk, &gmsk, &bmsk);
      decode_mask (rmsk, &rpos, &rsiz);
      decode_mask (gmsk, &gpos, &gsiz);
      decode_mask (bmsk, &bpos, &bsiz);

      for (i = 0; i < 256; i++)
        {
          spread_map[0][i] = spread_bits (i, rsiz);
          spread_map[1][i] = spread_bits (i, gsiz);
          spread_map[2][i] = spread_bits (i, bsiz);
        }
    }

  scale = (fromw > fromh
           ? (float) fromw / w
           : (float) fromh / h);

  /* Scale the pixmap from window size to Apple][ screen size (but 32bpp)
   */
  for (y = 0; y < h-1; y++)     /* iterate over dest pixels */
    for (x = 0; x < w-1; x++)
      {
        int xx, yy;
        unsigned int r=0, g=0, b=0;

        int xx1 = x * scale + fromx;
        int yy1 = y * scale + fromy;
        int xx2 = (x+1) * scale + fromx;
        int yy2 = (y+1) * scale + fromy;

        /* Iterate over the source pixels contributing to this one, and sum. */
        for (xx = xx1; xx < xx2; xx++)
          for (yy = yy1; yy < yy2; yy++)
            {
              unsigned char rr, gg, bb;
              unsigned long sp = ((xx > in->width || yy > in->height)
                                  ? 0 : XGetPixel (in, xx, yy));
              if (colors)
                {
                  rr = colors[sp].red   & 0xFF;
                  gg = colors[sp].green & 0xFF;
                  bb = colors[sp].blue  & 0xFF;
                }
              else
                {
                  rr = (sp & rmsk) >> rpos;
                  gg = (sp & gmsk) >> gpos;
                  bb = (sp & bmsk) >> bpos;
                  rr = spread_map[0][rr];
                  gg = spread_map[1][gg];
                  bb = spread_map[2][bb];
                }
              r += rr;
              g += gg;
              b += bb;
            }

        /* Scale summed pixel values down to 8/8/8 range */
        i = (xx2 - xx1) * (yy2 - yy1);
        if (i < 1) i = 1;
        r /= i;
        g /= i;
        b /= i;

        out[y * w + x] = (r << 16) | (g << 8) | b;
      }
}


/* Convert an XImage (of any size/depth/visual) to a 32bpp RGB array.
   Picks a random sub-image out of the source image, and scales it to WxH.
 */
static void
pick_a2_subimage (Display *dpy, Window window, XImage *in,
                  unsigned int *out, int w, int h)
{
  int fromx, fromy, fromw, fromh;
  if (in->width <= w || in->height <= h)
    {
      fromx = 0;
      fromy = 0;
      fromw = in->width;
      fromh = in->height;
    }
  else
    {
      int dw, dh;
      do {
        double scale = (0.5 + frand(0.7) + frand(0.7) + frand(0.7));
        fromw = w * scale;
        fromh = h * scale;
      } while (fromw > in->width ||
               fromh > in->height);

      dw = (in->width  - fromw) / 2;   /* near the center! */
      dh = (in->height - fromh) / 2;

      fromx = (dw <= 0 ? 0 : (random() % dw) + (dw/2));
      fromy = (dh <= 0 ? 0 : (random() % dh) + (dh/2));
    }

  scale_image (dpy, window, in,
               fromx, fromy, fromw, fromh,
               out, w, h);
}


/* Floyd-Steinberg dither.  Derived from ppmquant.c,
   Copyright (c) 1989, 1991 by Jef Poskanzer.
 */
static void
a2_dither (unsigned int *in, unsigned char *out, int w, int h)
{
  /*
    Apple ][ color map. Each pixel can only be 1 or 0, but what that
    means depends on whether it's an odd or even pixel, and whether
    the high bit in the byte is set or not. If it's 0, it's always
    black.
   */
  static const int a2_cmap[2][2][3] = {
    {
      /* hibit=0 */
      {/* odd pixels = blue */    0x00, 0x80, 0xff},
      {/* even pixels = red */    0xff, 0x80, 0x00}
    },
    {
      /* hibit=1 */
      {/* even pixels = purple */ 0xa0, 0x40, 0xa0},
      {/* odd pixels = green */   0x40, 0xff, 0x40}
    }
  };

  int x, y;
  unsigned int **pixels;
  unsigned int *pP;
  int maxval = 255;
  long *this_rerr;
  long *next_rerr;
  long *this_gerr;
  long *next_gerr;
  long *this_berr;
  long *next_berr;
  long *temp_err;
  int fs_scale = 1024;
  int brightness = 75;

#if 0
  {
    FILE *pipe = popen ("xv -", "w");
    fprintf (pipe, "P6\n%d %d\n%d\n", w, h, 255);
    for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
        {
          unsigned int p = in[y * w + x];
          unsigned int r = (p >> 16) & 0xFF;
          unsigned int g = (p >>  8) & 0xFF;
          unsigned int b = (p      ) & 0xFF;
          fprintf(pipe, "%c%c%c", r, g, b);
        }
    fclose (pipe);
  }
#endif

  /* Initialize Floyd-Steinberg error vectors. */
  this_rerr = (long *) calloc (w + 2, sizeof(long));
  next_rerr = (long *) calloc (w + 2, sizeof(long));
  this_gerr = (long *) calloc (w + 2, sizeof(long));
  next_gerr = (long *) calloc (w + 2, sizeof(long));
  this_berr = (long *) calloc (w + 2, sizeof(long));
  next_berr = (long *) calloc (w + 2, sizeof(long));


  /* #### do we really need more than one element of "pixels" at once?
   */
  pixels = (unsigned int **) malloc (h * sizeof (unsigned int *));
  for (y = 0; y < h; y++)
    pixels[y] = (unsigned int *) malloc (w * sizeof (unsigned int));

  for (x = 0; x < w + 2; ++x)
    {
      this_rerr[x] = random() % (fs_scale * 2) - fs_scale;
      this_gerr[x] = random() % (fs_scale * 2) - fs_scale;
      this_berr[x] = random() % (fs_scale * 2) - fs_scale;
      /* (random errors in [-1 .. 1]) */
    }

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      pixels[y][x] = in[y * w + x];

  for (y = 0; y < h; y++)
    {
      int xbyte;
      int err;
      int prev_byte=0;

      for (x = 0; x < w + 2; x++)
        next_rerr[x] = next_gerr[x] = next_berr[x] = 0;

      /* It's too complicated to go back and forth on alternate rows,
         so we always go left-right here. It doesn't change the result
         very much.

         For each group of 7 pixels, we have to try it both with the
         high bit=0 and =1. For each high bit value, we add up the
         total error and pick the best one.

         Because we have to go through each group of bits twice, we
         don't propagate the error values through this_[rgb]err since
         it would add them twice. So we keep seperate local_[rgb]err
         variables for propagating error within the 7-pixel group.
      */

      pP = pixels[y];
      for (xbyte=0; xbyte<280; xbyte+=7)
        {
          int best_byte=0;
          int best_error=2000000000;
          int hibit;
          int sr, sg, sb;
          int r2, g2, b2;
          int local_rerr=0, local_gerr=0, local_berr=0;

          for (hibit=0; hibit<2; hibit++)
            {
              int byte = hibit<<7;
              int tot_error=0;

              for (x=xbyte; x<xbyte+7; x++)
                {
                  int dist0, dist1;

                  /* Use Floyd-Steinberg errors to adjust actual color. */
                  sr = ((pP[x] >> 16) & 0xFF) * brightness/256;
                  sg = ((pP[x] >>  8) & 0xFF) * brightness/256;
                  sb = ((pP[x]      ) & 0xFF) * brightness/256;
                  sr += (this_rerr[x + 1] + local_rerr) / fs_scale;
                  sg += (this_gerr[x + 1] + local_gerr) / fs_scale;
                  sb += (this_berr[x + 1] + local_berr) / fs_scale;

                  if  (sr < 0) sr = 0;
                  else if  (sr > maxval) sr = maxval;
                  if  (sg < 0) sg = 0;
                  else if  (sg > maxval) sg = maxval;
                  if  (sb < 0) sb = 0;
                  else if  (sb > maxval) sb = maxval;

                  /* This is the color we'd get if we set the bit 1. For 0,
                     we get black */
                  r2=a2_cmap[hibit][x&1][0];
                  g2=a2_cmap[hibit][x&1][1];
                  b2=a2_cmap[hibit][x&1][2];

                  /*
                     dist0 and dist1 are the error (Minkowski 2-metric
                     distances in the color space) for choosing 0 and
                     1 respectively. 0 is black, 1 is the color r2,g2,b2.
                  */
                  dist1= (sr-r2)*(sr-r2) + (sg-g2)*(sg-g2) + (sb-b2)*(sb-b2);
                  dist0= sr*sr + sg*sg + sb*sb;

                  if (dist1<dist0)
                    {
                      byte |= 1 << (x-xbyte);
                      tot_error += dist1;

                      /* Wanted sr but got r2, so propagate sr-r2 */
                      local_rerr =  (sr - r2) * fs_scale * 7/16;
                      local_gerr =  (sg - g2) * fs_scale * 7/16;
                      local_berr =  (sb - b2) * fs_scale * 7/16;
                    }
                  else
                    {
                      tot_error += dist0;

                      /* Wanted sr but got 0, so propagate sr */
                      local_rerr =  sr * fs_scale * 7/16;
                      local_gerr =  sg * fs_scale * 7/16;
                      local_berr =  sb * fs_scale * 7/16;
                    }
                }

              if (tot_error < best_error)
                {
                  best_byte = byte;
                  best_error = tot_error;
                }
            }

          /* Avoid alternating 7f and ff in all-white areas, because it makes
             regular pink vertical lines */
          if ((best_byte&0x7f)==0x7f && (prev_byte&0x7f)==0x7f)
            best_byte=prev_byte;
          prev_byte=best_byte;

        /*
          Now that we've chosen values for all 8 bits of the byte, we
          have to fill in the real pixel values into pP and propagate
          all the error terms. We end up repeating a lot of the code
          above.
         */

        for (x=xbyte; x<xbyte+7; x++)
          {
            int bit=(best_byte>>(x-xbyte))&1;
            hibit=(best_byte>>7)&1;

            sr = (pP[x] >> 16) & 0xFF;
            sg = (pP[x] >>  8) & 0xFF;
            sb = (pP[x]      ) & 0xFF;
            sr += this_rerr[x + 1] / fs_scale;
            sg += this_gerr[x + 1] / fs_scale;
            sb += this_berr[x + 1] / fs_scale;

            if  (sr < 0) sr = 0;
            else if  (sr > maxval) sr = maxval;
            if  (sg < 0) sg = 0;
            else if  (sg > maxval) sg = maxval;
            if  (sb < 0) sb = 0;
            else if  (sb > maxval) sb = maxval;

            r2=a2_cmap[hibit][x&1][0] * bit;
            g2=a2_cmap[hibit][x&1][1] * bit;
            b2=a2_cmap[hibit][x&1][2] * bit;

            pP[x] = (r2<<16) | (g2<<8) | (b2);

            /* Propagate Floyd-Steinberg error terms. */
            err =  (sr - r2) * fs_scale;
            this_rerr[x + 2] +=  (err * 7) / 16;
            next_rerr[x    ] +=  (err * 3) / 16;
            next_rerr[x + 1] +=  (err * 5) / 16;
            next_rerr[x + 2] +=  (err    ) / 16;
            err =  (sg - g2) * fs_scale;
            this_gerr[x + 2] +=  (err * 7) / 16;
            next_gerr[x    ] +=  (err * 3) / 16;
            next_gerr[x + 1] +=  (err * 5) / 16;
            next_gerr[x + 2] +=  (err    ) / 16;
            err =  (sb - b2) * fs_scale;
            this_berr[x + 2] +=  (err * 7) / 16;
            next_berr[x    ] +=  (err * 3) / 16;
            next_berr[x + 1] +=  (err * 5) / 16;
            next_berr[x + 2] +=  (err    ) / 16;
          }

        /*
          And put the actual byte into out.
        */

        out[y*(w/7) + xbyte/7] = best_byte;

        }

      temp_err  = this_rerr;
      this_rerr = next_rerr;
      next_rerr = temp_err;
      temp_err  = this_gerr;
      this_gerr = next_gerr;
      next_gerr = temp_err;
      temp_err  = this_berr;
      this_berr = next_berr;
      next_berr = temp_err;
    }

  free (this_rerr);
  free (next_rerr);
  free (this_gerr);
  free (next_gerr);
  free (this_berr);
  free (next_berr);

  for (y=0; y<h; y++)
    free (pixels[y]);
  free (pixels);

#if 0
  {
    /* let's see what we got... */
    FILE *pipe = popen ("xv -", "w");
    fprintf (pipe, "P6\n%d %d\n%d\n", w, h, 255);
    for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
        {
          unsigned int r = (pixels[y][x]>>16)&0xff;
          unsigned int g = (pixels[y][x]>>8)&0xff;
          unsigned int b = (pixels[y][x]>>0)&0xff;
          fprintf(pipe, "%c%c%c", r, g, b);
        }
    fclose (pipe);
  }
#endif
}

typedef struct slideshow_data_s {
  int slideno;
  int render_img_lineno;
  unsigned char *render_img;
  char *img_filename;
  Bool image_loading_p;
} slideshow_data;


static void
image_loaded_cb (Screen *screen, Window window, Drawable p,
                 const char *name, XRectangle *geometry,
                 void *closure)
{
  Display *dpy = DisplayOfScreen (screen);
  apple2_sim_t *sim = (apple2_sim_t *) closure;
  slideshow_data *mine = (slideshow_data *) sim->controller_data;
  XWindowAttributes xgwa;
  int w = 280;
  int h = 192;
  XImage *image;
  unsigned int  *buf32 = (unsigned int  *) calloc (w, h * 4);
  unsigned char *buf8  = (unsigned char *) calloc (w/7, h);

  if (!buf32 || !buf8)
    {
      fprintf (stderr, "%s: out of memory (%dx%d)\n", progname, w, h);
      exit (1);
    }

  XGetWindowAttributes (dpy, window, &xgwa);

  image = XGetImage (dpy, p, 0, 0, xgwa.width, xgwa.height, ~0, ZPixmap);
  XFreePixmap (dpy, p);
  p = 0;

  /* Scale the XImage down to Apple][ size, and convert it to a 32bpp
     image (regardless of whether it started as TrueColor/PseudoColor.)
   */
  pick_a2_subimage (dpy, window, image, buf32, w, h);
  free(image->data);
  image->data = 0;
  XDestroyImage(image);

  /* Then dither the 32bpp image to a 6-color Apple][ colormap.
   */
  a2_dither (buf32, buf8, w, h);

  free (buf32);

  mine->image_loading_p = False;
  mine->img_filename = (name ? strdup (name) : 0);
  mine->render_img = buf8;
}



static const char *apple2_defaults [] = {
  ".background:		   black",
  ".foreground:		   white",
  "*mode:		   random",
  "*duration:		   60",
  "*program:		   xscreensaver-text --cols 40",
  "*metaSendsESC:	   True",
  "*swapBSDEL:		   True",
  "*fast:		   False",
# ifdef HAVE_FORKPTY
  "*usePty:                True",
#else
  "*usePty:                False",
# endif /* !HAVE_FORKPTY */

  ANALOGTV_DEFAULTS
  0
};

static XrmOptionDescRec apple2_options [] = {
  { "-mode",		".mode",		XrmoptionSepArg, 0 },
  { "-slideshow",	".mode",		XrmoptionNoArg,  "slideshow" },
  { "-basic",	        ".mode",		XrmoptionNoArg,  "basic" },
  { "-text",     	".mode",		XrmoptionNoArg,  "text" },
  { "-program",		".program",		XrmoptionSepArg, 0 },
  { "-duration",	".duration",		XrmoptionSepArg, 0 },
  { "-pty",		".usePty",		XrmoptionNoArg,  "True"  },
  { "-pipe",		".usePty",		XrmoptionNoArg,  "False" },
  { "-meta",		".metaSendsESC",	XrmoptionNoArg,  "False" },
  { "-esc",		".metaSendsESC",	XrmoptionNoArg,  "True"  },
  { "-bs",		".swapBSDEL",		XrmoptionNoArg,  "False" },
  { "-del",		".swapBSDEL",		XrmoptionNoArg,  "True"  },
  { "-fast",		".fast",		XrmoptionNoArg,  "True"  },
  ANALOGTV_OPTIONS
  { 0, 0, 0, 0 }
};

/*
  TODO: this should load 10 images at startup time, then cycle through them
  to avoid the pause while it loads.
 */

static void slideshow_controller(apple2_sim_t *sim, int *stepno,
                                 double *next_actiontime)
{
  apple2_state_t *st=sim->st;
  int i;
  slideshow_data *mine;

  if (!sim->controller_data)
    sim->controller_data = calloc (1, sizeof(*mine));
  mine = (slideshow_data *) sim->controller_data;

  switch(*stepno) {

  case 0:
    a2_invalidate(st);
    a2_clear_hgr(st);
    a2_cls(st);
    sim->typing_rate = 0.3;
    sim->dec->powerup=0.0;

    a2_goto(st, 0, 16);
    a2_prints(st, "APPLE ][");
    a2_goto(st,23,0);
    a2_printc(st,']');

    *stepno=10;
    break;

  case 10:
    {
      XWindowAttributes xgwa;
      Pixmap p;
      XGetWindowAttributes (sim->dpy, sim->window, &xgwa);
      p = XCreatePixmap (sim->dpy, sim->window, xgwa.width, xgwa.height, 
                         xgwa.depth);
      mine->image_loading_p = True;
      load_image_async (xgwa.screen, sim->window, p, image_loaded_cb, sim);

      /* pause with a blank screen for a bit, while the image loads in the
         background. */
      *next_actiontime += 2.0;
      *stepno=11;
    }
    break;

  case 11:
    if (! mine->image_loading_p) {  /* image is finally loaded */
      if (st->gr_mode) {
        *stepno=30;
      } else {
        *stepno=20;
      }
      *next_actiontime += 3.0;
    }
    break;

  case 20:
    sim->typing="HGR\n";
    *stepno=29;
    break;

  case 29:
    sim->printing="]";
    *stepno=30;
    break;

  case 30:
    st->gr_mode=A2_GR_HIRES;
    if (mine->img_filename) {
      char *basename, *tmp;
      char *s;

      basename = tmp = strdup (mine->img_filename);
      while (1)
        {
          char *slash = strchr(basename, '/');
          if (!slash || !slash[1]) break;
          basename = slash+1;
        }
      {
        char *dot=strrchr(basename,'.');
        if (dot) *dot=0;
      }
      if (strlen(basename)>20) basename[20]=0;
      for (s=basename; *s; s++) {
        *s = toupper (*s);
        if (*s <= ' ') *s = '_';
      }
      sprintf(sim->typing_buf, "BLOAD %s\n", basename);
      sim->typing = sim->typing_buf;

      free(tmp);
    } else {
      sim->typing = "BLOAD IMAGE\n";
    }
    mine->render_img_lineno=0;

    *stepno=35;
    break;

  case 35:
    *next_actiontime += 0.7;
    *stepno=40;
    break;

  case 40:
    if (mine->render_img_lineno>=192) {
      sim->printing="]";
      sim->typing="POKE 49234,0\n";
      *stepno=50;
      return;
    }

    for (i=0; i<6 && mine->render_img_lineno<192; i++) {
      a2_display_image_loading(st, mine->render_img,
                               mine->render_img_lineno++);
    }

    /* The disk would have to seek every 13 sectors == 78 lines.
       (This ain't no newfangled 16-sector operating system) */
    if ((mine->render_img_lineno%78)==0) {
      *next_actiontime += 0.5;
    } else {
      *next_actiontime += 0.08;
    }
    break;

  case 50:
    st->gr_mode |= A2_GR_FULL;
    *stepno=60;
    /* Note that sim->delay is sometimes "infinite" in this controller.
       These images are kinda dull anyway, so don't leave it on too long. */
    *next_actiontime += 2;
    break;

  case 60:
    sim->printing="]";
    sim->typing="POKE 49235,0\n";
    *stepno=70;
    break;

  case 70:
    sim->printing="]";
    st->gr_mode &= ~A2_GR_FULL;
    if (mine->render_img) {
      free(mine->render_img);
      mine->render_img=NULL;
    }
    if (mine->img_filename) {
      free(mine->img_filename);
      mine->img_filename=NULL;
    }
    *stepno=10;
    break;

  case 80:
    /* Do nothing, just wait */
    *next_actiontime += 2.0;
    *stepno = A2CONTROLLER_FREE;
    break;

  case A2CONTROLLER_FREE:
    /* It is possible that still image is being loaded,
       in that case mine cannot be freed, because
       callback function tries to use it, so wait.
    */
    if (mine->image_loading_p) {
      *stepno = 80;
      break;
    }
    free(mine->render_img);
    free(mine->img_filename);
    free(mine);
    mine = 0;
    return;

  }
}

#define NPAR 16

#define COUNTDOWN_BANNER   "intermission    "
#define COUNTDOWN_DURATION 18*60+30
#undef COUNTDOWN_BANNER

struct terminal_controller_data {
  Display *dpy;
  char curword[256];
  unsigned char lastc;
  double last_emit_time;
  time_t launch_time;
  text_data *tc;
  ansi_tty *tty;

  int escstate;
  int csiparam[NPAR];
  int curparam;
  int cursor_x, cursor_y;
  int saved_x,  saved_y;
  union {
    struct {
      unsigned int bold : 1;
      unsigned int blink : 1;
      unsigned int rev : 1;
    } bf;
    int w;
  } termattrib;
  Bool fast_p;

};

static void a2_tty_send (void *, const char *);


/* The structure of closure linkage throughout this code is so amazingly
   baroque that I can't get to the 'struct state' from where I need it. */
static char *global_program;
static Bool global_fast_p;


static void
terminal_closegen(struct terminal_controller_data *mine)
{
  if (mine->tc) {
    textclient_close (mine->tc);
    mine->tc = 0;
  }

  if (mine->tty)
    ansi_tty_free (mine->tty);
}

static int
terminal_read(struct terminal_controller_data *mine, unsigned char *buf, int n)
{
  if (!mine || !mine->tc) {
    return 0;
  } else {
    int i, count = 0;
    for (i = 0; i < n; i++) {
      int c = textclient_getc (mine->tc);
      if (c <= 0) break;
      buf[i] = c;
      mine->lastc = c;
      count++;
    }
    return count;
  }
}


static int
terminal_keypress_handler (Display *dpy, XEvent *event, void *data)
{
  struct terminal_controller_data *mine =
    (struct terminal_controller_data *) data;
  mine->dpy = dpy;
  if (event->xany.type == KeyPress && mine->tc)
    return textclient_putc_event (mine->tc, &event->xkey);
  return 0;
}


#ifdef COUNTDOWN_BANNER
static void
a2_draw_banner (apple2_state_t *st, struct terminal_controller_data *mine)
{
  int x, y;
  int w = 40;
  int h = 3;
  char buf[41], *s;
  struct timeval tv;
  double t;

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      st->textlines[y][x] = ' ' | 0xC0;

# ifdef GETTIMEOFDAY_TWO_ARGS
  gettimeofday (&tv, NULL);
# else
  gettimeofday (&tv);
# endif

  t = ((mine->launch_time + COUNTDOWN_DURATION)  - 
       (tv.tv_sec + ((double) tv.tv_usec * 0.000001)));
  if (t < 0) t = 0;
  sprintf (buf, "%.30s %d:%02d:%02d.%1d",
           COUNTDOWN_BANNER,
           (int) (t/60/60),
           (int) (t/60) % 60,
           (int) t % 60,
           (int) (t * 10) % 10);
  x = (w - strlen (buf)) / 2;
  y = 0;
  s = buf;
  while (*s && x < w) {
    char c = *s++;
    if (c >= 'a' && c <= 'z') c &= 0xDF;
    c |= 0xC0;
    st->textlines[y][x] = c;
    x++;
  }
}
#endif /* COUNTDOWN_BANNER */


static void
a2_ascii_printc (apple2_state_t *st, unsigned char c,
                 Bool bold_p, Bool blink_p, Bool rev_p,
                 Bool scroll_p)
{
  if (c >= 'a' && c <= 'z')            /* upcase lower-case chars */
    {
      c &= 0xDF;
    }
  else if ((c >= 'A'+128) ||                    /* upcase and blink */
           (c < ' ' && c != 014 &&              /* high-bit & ctl chrs */
            c != '\r' && c != '\n' && c!='\t'))
    {
      c = (c & 0x1F) | 0x80;
    }
  else if (c >= 'A' && c <= 'Z')            /* invert upper-case chars */
    {
# ifndef COUNTDOWN_BANNER
      c |= 0x80;
# endif
    }

  if (bold_p)  c |= 0xc0;
  if (blink_p) c = (c & ~0x40) | 0x80;
  if (rev_p)   c |= 0xc0;

  if (scroll_p)
    a2_printc(st, c);
  else
    a2_printc_noscroll(st, c);
}


static void
a2_vt100_printc (apple2_sim_t *sim, struct terminal_controller_data *state,
                 unsigned char c)
{
  apple2_state_t *st=sim->st;
  int w = SCREEN_COLS;
  int h = SCREEN_ROWS;
  int x, y;
  ansi_tty *tty = state->tty;
  ansi_tty_print (tty, c);

  /* We could optimize this to not re-draw characters that haven't changed,
     but it seems plenty fast enough. */

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
        tty_char *tc = &tty->grid [tty->width * y + x];
        unsigned char ascii = 0;
        tty_flag flag = tc->flags;
        int inv_p = flag & (TTY_INVERSE | TTY_BOLD);
        if (tty->inverse_p) inv_p = !inv_p;

        if (tc->c <= 128)
          ascii = tc->c;
        else
          {
            /* Convert non-ASCII Unicode to closest ASCII. */
            char utf8[10];
            int L = utf8_encode (tc->c, utf8, sizeof(utf8)-1);
            utf8[L] = 0;
            ascii = '?';
            if (L)
              {
                char *s = utf8_to_latin1 (utf8, True);
                if (s)
                  {
                    ascii = s[0];
                    free (s);
                  }
              }
          }

        if (! ascii) ascii = ' ';

        a2_goto (st, y, x);

        if (flag & TTY_SYMBOLS)    /* Convert to the nearest ASCII */
          switch (ascii) {
          case 0x60: ascii = '*';  /* ◆ */  flag &= ~TTY_SYMBOLS; break;
          case 0x6A: ascii = 'J';  /* ┘ */  flag &= ~TTY_SYMBOLS; break;
          case 0x6B: ascii = 'T';  /* ┐ */  flag &= ~TTY_SYMBOLS; break;
          case 0x6C: ascii = 'r';  /* ┌ */  flag &= ~TTY_SYMBOLS; break;
          case 0x6D: ascii = 'L';  /* └ */  flag &= ~TTY_SYMBOLS; break;
          case 0x6E: ascii = '+';  /* ┼ */  flag &= ~TTY_SYMBOLS; break;
          case 0x6F: ascii = '-';  /* ⎺ */  flag &= ~TTY_SYMBOLS; break;
          case 0x70: ascii = '-';  /* ⎻ */  flag &= ~TTY_SYMBOLS; break;
          case 0x71: ascii = '=';  /* ─ */  flag &= ~TTY_SYMBOLS; break;
          case 0x72: ascii = '_';  /* ⎼ */  flag &= ~TTY_SYMBOLS; break;
          case 0x73: ascii = '_';  /* ⎽ */  flag &= ~TTY_SYMBOLS; break;
          case 0x74: ascii = 'F';  /* ├ */  flag &= ~TTY_SYMBOLS; break;
          case 0x75: ascii = '+';  /* ┤ */  flag &= ~TTY_SYMBOLS; break;
          case 0x76: ascii = '+';  /* ┴ */  flag &= ~TTY_SYMBOLS; break;
          case 0x77: ascii = 'T';  /* ┬ */  flag &= ~TTY_SYMBOLS; break;
          case 0x78: ascii = '#';  /* │ */  flag &= ~TTY_SYMBOLS; break;
          }


        if (flag & TTY_SYMBOLS)
          /* Draw unknown symbol font characters as a box. */
          a2_ascii_printc (st, ' ', 0, 0, !inv_p, False);
        else
          a2_ascii_printc (st, ascii,
                           0,
                           flag & TTY_BLINK,
                           inv_p,
                           False);
      }

  a2_goto (st, tty->y, tty->x);
}


/*
  It's fun to put things like "gdb" as the command. For one, it's
  amusing how the standard mumble (version, no warranty, it's
  GNU/Linux dammit) occupies an entire screen on the Apple ][.
*/

static void
terminal_controller(apple2_sim_t *sim, int *stepno, double *next_actiontime)
{
  apple2_state_t *st=sim->st;
  int c;
  int i;

  struct terminal_controller_data *mine;
  if (!sim->controller_data)
    sim->controller_data=calloc(sizeof(struct terminal_controller_data),1);
  mine=(struct terminal_controller_data *) sim->controller_data;
  mine->dpy = sim->dpy;
  if (!mine->launch_time) mine->launch_time = time ((time_t *) 0);

  mine->fast_p           = global_fast_p;

  switch(*stepno) {

  case 0:
# ifdef COUNTDOWN_BANNER
    if (1)
# else
    if (random()%2)
# endif
      st->gr_mode |= A2_GR_FULL; /* Turn on color mode even through it's
                                    showing text */
    a2_cls(st);
# ifndef COUNTDOWN_BANNER
    a2_goto(st,0,16);
    a2_prints(st, "APPLE ][");
    a2_goto(st,2,0);
# endif
    mine->cursor_y = 2;

    if (! mine->tc) {
      mine->tc = textclient_open (mine->dpy);
      textclient_reshape (mine->tc,
                          SCREEN_COLS, SCREEN_ROWS,
                          SCREEN_COLS, SCREEN_ROWS,
                          0);
    }

    if (!mine->tty) {
      mine->tty = ansi_tty_init (SCREEN_COLS, SCREEN_ROWS);
      mine->tty->closure  = mine;
      mine->tty->tty_send = a2_tty_send;
    }

    if (! mine->fast_p)
      *next_actiontime += 4.0;
    *stepno = 10;

    mine->last_emit_time = sim->curtime;
    break;

  case 10:
  case 11:
    {
      Bool first_line_p = (*stepno == 10);
      unsigned char buf[1024];
      int nr,nwant;
      double elapsed;

      elapsed=sim->curtime - mine->last_emit_time;

      nwant = elapsed * 25.0;   /* characters per second */

      if (first_line_p) {
        *stepno = 11;
        nwant = 1;
      }

      if (nwant > 40) nwant = 40;

      if (mine->fast_p)
        nwant = sizeof(buf)-1;

      if (nwant <= 0) break;

      mine->last_emit_time = sim->curtime;

      nr=terminal_read(mine, buf, nwant);
      for (i=0; i<nr; i++) {
        c=buf[i];

        if (mine->tc)
          a2_vt100_printc (sim, mine, c);
        else
          a2_ascii_printc (st, c, False, False, False, True);
      }

# ifdef COUNTDOWN_BANNER
      a2_draw_banner (st, mine);
# endif
    }
    break;

  case A2CONTROLLER_FREE:
    terminal_closegen(mine);
    free(mine);
    mine = 0;
    return;
  }
}

static void
a2_tty_send (void *closure, const char *text)
{
  struct terminal_controller_data *mine =
    (struct terminal_controller_data *) closure;
  textclient_puts (mine->tc, text);
}


struct basic_controller_data {
  int prog_line;
  int x,y,k;
  const char * const * progtext;
  int progstep;
  char *rep_str;
  int rep_pos;
  double prog_start_time;
  char error_buf[256];
};

/*
  Adding more programs is easy. Just add a listing here and to all_programs,
  then add the state machine to actually execute it to basic_controller.
 */
static const char * const moire_program[]={
  "10 HGR2\n",
  "20 FOR Y = 0 TO 190 STEP 2\n",
  "30 HCOLOR=4 : REM BLACK\n",
  "40 HPLOT 0,191-Y TO 279,Y\n",
  "60 HCOLOR=7 : REM WHITE\n",
  "80 HPLOT 0,190-Y TO 279,Y+1\n",
  "90 NEXT Y\n",
  "100 FOR X = 0 TO 278 STEP 3\n",
  "110 HCOLOR=4\n",
  "120 HPLOT 279-X,0 TO X,191\n",
  "140 HCOLOR=7\n",
  "150 HPLOT 278-X,0 TO X+1,191\n",
  "160 NEXT X\n",
  NULL
};

static const char * const sinewave_program[] = {
  "10 HGR\n",
  "25 K=0\n",
  "30 FOR X = 0 TO 279\n",
  "32 HCOLOR= 0\n",
  "35 HPLOT X,0 TO X,159\n",
  "38 HCOLOR= 3\n",
  "40 Y = 80 + SIN(15*(X-K)/279) * 40\n",
  "50 HPLOT X,Y\n",
  "60 NEXT X\n",
  "70 K=K+4\n",
  "80 GOTO 30\n",
  NULL
};

#if 0
static const char * const dumb_program[]={
  "10 PRINT \"APPLE ][ ROOLZ! TRS-80 DROOLZ!\"\n",
  "20 GOTO 10\n",
  NULL
};
#endif

static const char * const random_lores_program[]={
  "1 REM APPLE ][ SCREEN SAVER\n",
  "10 GR\n",
  "100 COLOR= RND(1)*16\n",

  "110 X=RND(1)*40\n",
  "120 Y1=RND(1)*40\n",
  "130 Y2=RND(1)*40\n",
  "140 FOR Y = Y1 TO Y2\n",
  "150 PLOT X,Y\n",
  "160 NEXT Y\n",

  "210 Y=RND(1)*40\n",
  "220 X1=RND(1)*40\n",
  "230 X2=RND(1)*40\n",
  "240 FOR X = X1 TO X2\n",
  "250 PLOT X,Y\n",
  "260 NEXT X\n",
  "300 GOTO 100\n",

  NULL
};

static char typo_map[256];

static int make_typo(char *out_buf, const char *orig, char *err_buf)
{
  int i,j;
  int errc;
  int success=0;
  err_buf[0]=0;

  typo_map['A']='Q';
  typo_map['S']='A';
  typo_map['D']='S';
  typo_map['F']='G';
  typo_map['G']='H';
  typo_map['H']='J';
  typo_map['J']='H';
  typo_map['K']='L';
  typo_map['L']=';';

  typo_map['Q']='1';
  typo_map['W']='Q';
  typo_map['E']='3';
  typo_map['R']='T';
  typo_map['T']='Y';
  typo_map['Y']='U';
  typo_map['U']='Y';
  typo_map['I']='O';
  typo_map['O']='P';
  typo_map['P']='[';

  typo_map['Z']='X';
  typo_map['X']='C';
  typo_map['C']='V';
  typo_map['V']='C';
  typo_map['B']='N';
  typo_map['N']='B';
  typo_map['M']='N';
  typo_map[',']='.';
  typo_map['.']=',';

  typo_map['!']='1';
  typo_map['@']='2';
  typo_map['#']='3';
  typo_map['$']='4';
  typo_map['%']='5';
  typo_map['^']='6';
  typo_map['&']='7';
  typo_map['*']='8';
  typo_map['(']='9';
  typo_map[')']='0';

  typo_map['1']='Q';
  typo_map['2']='W';
  typo_map['3']='E';
  typo_map['4']='R';
  typo_map['5']='T';
  typo_map['6']='Y';
  typo_map['7']='U';
  typo_map['8']='I';
  typo_map['9']='O';
  typo_map['0']='-';

  strcpy(out_buf, orig);
  for (i=0; out_buf[i]; i++) {
    char *p = out_buf+i;

    if (i>2 && p[-2]=='R' && p[-1]=='E' && p[0]=='M')
      break;

    if (isalpha(p[0]) &&
        isalpha(p[1]) &&
        p[0] != p[1] &&
        random()%15==0)
      {
        int tmp=p[1];
        p[1]=p[0];
        p[0]=tmp;
        success=1;
        sprintf(err_buf,"?SYNTAX ERROR\n");
        break;
      }

    if (random()%10==0 && strlen(p)>=4 && (errc=typo_map[(int)(unsigned char)p[0]])) {
      int remain=strlen(p);
      int past=random()%(remain-2)+1;
      memmove(p+past+past, p, remain+1);
      p[0]=errc;
      for (j=0; j<past; j++) {
        p[past+j]=010;
      }
      break;
    }
  }
  return success;
}

static const struct {
  const char * const * progtext;
  int progstep;
} all_programs[]={
  {moire_program, 100},
  /*{dumb_program, 200}, */
  {sinewave_program, 400},
  {random_lores_program, 500},
};

static void
basic_controller(apple2_sim_t *sim, int *stepno, double *next_actiontime)
{
  apple2_state_t *st=sim->st;
  int i;

  struct basic_controller_data *mine;
  if (!sim->controller_data)
    sim->controller_data=calloc(sizeof(struct basic_controller_data),1);
  mine=(struct basic_controller_data *) sim->controller_data;

  switch (*stepno) {
  case 0:
    st->gr_mode=0;
    a2_cls(st);
    a2_goto(st,0,16);
    a2_prints(st, "APPLE ][");
    a2_goto(st,23,0);
    a2_printc(st,']');
    sim->typing_rate=0.2;

    i=random()%countof(all_programs);
    mine->progtext=all_programs[i].progtext;
    mine->progstep=all_programs[i].progstep;
    mine->prog_line=0;

    *next_actiontime += 1.0;
    *stepno=10;
    break;

  case 10:
    if (st->cursx==0) a2_printc(st,']');
    if (mine->progtext[mine->prog_line]) {
      if (random()%4==0) {
        int err=make_typo(sim->typing_buf,
                          mine->progtext[mine->prog_line],
                          mine->error_buf);
        sim->typing=sim->typing_buf;
        if (err) {
          *stepno=11;
        } else {
          mine->prog_line++;
        }
      } else {
        sim->typing=mine->progtext[mine->prog_line++];
      }
    } else {
      *stepno=15;
    }
    break;

  case 11:
    sim->printing=mine->error_buf;
    *stepno=12;
    break;

  case 12:
    if (st->cursx==0) a2_printc(st,']');
    *next_actiontime+=1.0;
    *stepno=10;
    break;

  case 15:
    sim->typing="RUN\n";
    mine->y=0;
    mine->x=0;
    mine->k=0;
    mine->prog_start_time=*next_actiontime;
    *stepno=mine->progstep;
    break;

    /* moire_program */
  case 100:
    st->gr_mode=A2_GR_HIRES|A2_GR_FULL;
    for (i=0; i<24 && mine->y<192; i++)
      {
        a2_hline(st, 4, 0, 191-mine->y, 279, mine->y);
        a2_hline(st, 7, 0, 191-mine->y-1, 279, mine->y+1);
        mine->y += 2;
      }
    if (mine->y>=192) {
      mine->x = 0;
      *stepno = 110;
    }
    break;

  case 110:
    for (i=0; i<24 && mine->x<280; i++)
      {
        a2_hline(st, 4, 279-mine->x, 0, mine->x, 192);
        a2_hline(st, 7, 279-mine->x-1, 0, mine->x+1, 192);
        mine->x+=3;
      }
    if (mine->x >= 280) *stepno=120;
    break;

  case 120:
    if (*next_actiontime > mine->prog_start_time+sim->delay) *stepno=999;
    break;

    /* dumb_program */
  case 200:
    mine->rep_str="\nAPPLE ][ ROOLZ! TRS-80 DROOLZ!";
    for (i=0; i<30; i++) {
      a2_prints(st, mine->rep_str);
    }
    *stepno=210;
    break;

  case 210:
    i=random()%strlen(mine->rep_str);
    while (mine->rep_pos != i) {
      a2_printc(st, mine->rep_str[mine->rep_pos]);
      mine->rep_pos++;
      if (!mine->rep_str[mine->rep_pos]) mine->rep_pos=0;
    }
    if (*next_actiontime > mine->prog_start_time+sim->delay) *stepno=999;
    break;

    /* sinewave_program */
  case 400:
    st->gr_mode=A2_GR_HIRES;
    *stepno=410;
    break;

  case 410:
    for (i=0; i<48; i++) {
      int y=80 + (int)(75.0*sin(15.0*(mine->x-mine->k)/279.0));
      a2_hline(st, 0, mine->x, 0, mine->x, 159);
      a2_hplot(st, 3, mine->x, y);
      mine->x += 1;
      if (mine->x>=279) {
        mine->x=0;
        mine->k+=4;
      }
    }
    if (*next_actiontime > mine->prog_start_time+sim->delay) *stepno=999;
    break;

  case 420:
    a2_prints(st, "]");
    *stepno=999;
    break;

    /* random_lores_program */
  case 500:
    st->gr_mode=A2_GR_LORES|A2_GR_FULL;
    a2_clear_gr(st);
    *stepno=510;

  case 510:
    for (i=0; i<10; i++) {
      int color,x,y,x1,x2,y1,y2;

      color=random()%15;
      x=random()%40;
      y1=random()%48;
      y2=random()%48;
      for (y=y1; y<y2; y++) a2_plot(st, color, x, y);

      x1=random()%40;
      x2=random()%40;
      y=random()%48;
      for (x=x1; x<x2; x++) a2_plot(st, color, x, y);
    }
    if (*next_actiontime > mine->prog_start_time+sim->delay) *stepno=999;
    break;

  case 999:
    *stepno=0;
    break;

  case A2CONTROLLER_FREE:
    free(mine);
    mine = 0;
    break;
  }

}

static void (* const controllers[]) (apple2_sim_t *sim, int *stepno,
                                     double *next_actiontime) = {
  slideshow_controller,
  terminal_controller,
  basic_controller
};

struct state {
  int duration;
  Bool random_p;
  apple2_sim_t *sim;
  void (*controller) (apple2_sim_t *sim, int *stepno, double *next_actiontime);
};


static void *
apple2_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  char *s;

  st->duration = get_integer_resource (dpy, "duration", "Integer");

  st->controller = 0;
  if (st->duration < 1) st->duration = 1;

  s = get_string_resource (dpy, "mode", "Mode");
  if (!s || !*s || !strcasecmp(s, "random"))
    st->random_p = True;
  else if (!strcasecmp(s, "text"))
     st->controller = terminal_controller;
  else if (!strcasecmp(s, "slideshow"))
     st->controller = slideshow_controller;
  else if (!strcasecmp(s, "basic"))
     st->controller = basic_controller;
  else
    {
      fprintf (stderr, "%s: mode must be text, slideshow, or random; not %s\n",
               progname, s);
      exit (1);
    }
  if (s) free (s);

  if (!global_program) {
    global_program = get_string_resource (dpy, "program", "Program");
    global_fast_p = get_boolean_resource (dpy, "fast", "Boolean");
  }


  /* Kludge for MacOS standalone mode: see OSX/SaverRunner.m. */
  {
    const char *s = getenv ("XSCREENSAVER_STANDALONE");
    if (s && *s && strcmp(s, "0"))
      {
        st->controller = terminal_controller;
        st->random_p   = False;
        if (global_program) free (global_program);
        global_program = strdup (getenv ("SHELL"));
        global_fast_p  = True;
      }
  }


  if (! st->random_p) {
    if (st->controller == terminal_controller ||
        st->controller == slideshow_controller)
      st->duration = 999999;  /* these run "forever" */
  }

  return st;
}

static unsigned long
apple2_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (! st->sim) {
    if (st->random_p)
      st->controller = controllers[random() % (countof(controllers))];
    st->sim = apple2_start (dpy, window, st->duration, st->controller);
  }

  if (! apple2_one_frame (st->sim)) {
    st->sim = 0;
  }

#ifdef HAVE_MOBILE
  return 0;
#else
  return 5000;
#endif
}

static void
apple2_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  if (st->sim)
    analogtv_reconfigure (st->sim->dec);
}

static Bool
apple2_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;

  if (st->sim &&
      st->controller == terminal_controller &&
      event->xany.type == KeyPress) {
    terminal_keypress_handler (dpy, event, st->sim->controller_data);
    return True;
  }

  return False;
}

static void
apple2_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->sim) {
    st->sim->stepno = A2CONTROLLER_DONE;
    if (apple2_one_frame (st->sim))
      abort();  /* should have freed! */
  }
  if (global_program) free (global_program);
  global_program = 0;
  free (st);
}


XSCREENSAVER_MODULE ("Apple2", apple2)
