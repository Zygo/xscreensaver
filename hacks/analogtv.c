/* analogtv, Copyright (c) 2003, 2004 Trevor Blackwell <tlb@tlb.org>
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

  This is the code for implementing something that looks like a conventional
  analog TV set. It simulates the following characteristics of standard
  televisions:

  - Realistic rendering of a composite video signal
  - Compression & brightening on the right, as the scan gets truncated
    because of saturation in the flyback transformer
  - Blooming of the picture dependent on brightness
  - Overscan, cutting off a few pixels on the left side.
  - Colored text in mixed graphics/text modes

  It's amazing how much it makes your high-end monitor look like at large
  late-70s TV. All you need is to put a big "Solid State" logo in curly script
  on it and you'd be set.

  In DirectColor or TrueColor modes, it generates pixel values
  directly from RGB values it calculates across each scan line. In
  PseudoColor mode, it consider each possible pattern of 5 preceding
  bit values in each possible position modulo 4 and allocates a color
  for each. A few things, like the brightening on the right side as
  the horizontal trace slows down, aren't done in PseudoColor.

  I originally wrote it for the Apple ][ emulator, and generalized it
  here for use with a rewrite of xteevee and possibly others.

  A maxim of technology is that failures reveal underlying mechanism.
  A good way to learn how something works is to push it to failure.
  The way it fails will usually tell you a lot about how it works. The
  corollary for this piece of software is that in order to emulate
  realistic failures of a TV set, it has to work just like a TV set.
  So there is lots of DSP-style emulation of analog circuitry for
  things like color decoding, H and V sync following, and more. In
  2003, computers are just fast enough to do this at television signal
  rates. We use a 14 MHz sample rate here, so we can do on the order
  of a couple hundred instructions per sample and keep a good frame
  rate.

  Trevor Blackwell <tlb@tlb.org>
*/

#ifdef HAVE_COCOA
# include "jwxyz.h"
#else /* !HAVE_COCOA */
# include <X11/Xlib.h>
# include <X11/Xutil.h>
#endif

#include <assert.h>
#include "utils.h"
#include "resources.h"
#include "analogtv.h"
#include "yarandom.h"
#include "grabscreen.h"

/* #define DEBUG 1 */

#ifdef DEBUG
/* only works on linux + freebsd */
#include <machine/cpufunc.h>

#define DTIME_DECL u_int64_t dtimes[100]; int n_dtimes
#define DTIME_START do {n_dtimes=0; dtimes[n_dtimes++]=rdtsc(); } while (0)
#define DTIME dtimes[n_dtimes++]=rdtsc()
#define DTIME_SHOW(DIV) \
do { \
  double _dtime_div=(DIV); \
  printf("time/%.1f: ",_dtime_div); \
  for (i=1; i<n_dtimes; i++) \
    printf(" %0.9f",(dtimes[i]-dtimes[i-1])* 1e-9 / _dtime_div); \
  printf("\n"); \
} while (0)

#else

#define DTIME_DECL
#define DTIME_START  do { } while (0)
#define DTIME  do { } while (0)
#define DTIME_SHOW(DIV)  do { } while (0)

#endif


#define FASTRND (fastrnd = fastrnd*1103515245+12345)

static void analogtv_ntsc_to_yiq(analogtv *it, int lineno, double *signal,
                                 int start, int end);

static double puramp(analogtv *it, double tc, double start, double over)
{
  double pt=it->powerup-start;
  double ret;
  if (pt<0.0) return 0.0;
  if (pt>900.0 || pt/tc>8.0) return 1.0;

  ret=(1.0-exp(-pt/tc))*over;
  if (ret>1.0) return 1.0;
  return ret*ret;
}

/*
  There are actual standards for TV signals: NTSC and RS-170A describe the
  system used in the US and Japan. Europe has slightly different systems, but
  not different enough to make substantially different screensaver displays.
  Sadly, the standards bodies don't do anything so useful as publish the spec on
  the web. Best bets are:

    http://www.ee.washington.edu/conselec/CE/kuhn/ntsc/95x4.htm
    http://www.ntsc-tv.com/ntsc-index-02.htm

  In DirectColor or TrueColor modes, it generates pixel values directly from RGB
  values it calculates across each scan line. In PseudoColor mode, it consider
  each possible pattern of 5 preceding bit values in each possible position
  modulo 4 and allocates a color for each. A few things, like the brightening on
  the right side as the horizontal trace slows down, aren't done in PseudoColor.

  I'd like to add a bit of visible retrace, but it conflicts with being able to
  bitcopy the image when fast scrolling. After another couple of CPU
  generations, we could probably regenerate the whole image from scratch every
  time. On a P4 2 GHz it can manage this fine for blinking text, but scrolling
  looks too slow.
*/

/* localbyteorder is MSBFirst or LSBFirst */
static int localbyteorder;
static const double float_low8_ofs=8388608.0;
static int float_extraction_works;

typedef union {
  float f;
  int i;
} float_extract_t;

static void
analogtv_init(void)
{
  int i;
  {
    unsigned int localbyteorder_loc = (MSBFirst<<24) | (LSBFirst<<0);
    localbyteorder=*(char *)&localbyteorder_loc;
  }

  if (1) {
    float_extract_t fe;
    int ans;

    float_extraction_works=1;
    for (i=0; i<256*4; i++) {
      fe.f=float_low8_ofs+(double)i;
      ans=fe.i&0x3ff;
      if (ans != i) {
#ifdef DEBUG
        printf("Float extraction failed for %d => %d\n",i,ans);
#endif
        float_extraction_works=0;
        break;
      }
    }
  }

}

void
analogtv_set_defaults(analogtv *it, char *prefix)
{
  char buf[256];

  sprintf(buf,"%sTVTint",prefix);
  it->tint_control = get_float_resource(it->dpy, buf,"TVTint");
  sprintf(buf,"%sTVColor",prefix);
  it->color_control = get_float_resource(it->dpy, buf,"TVColor")/100.0;
  sprintf(buf,"%sTVBrightness",prefix);
  it->brightness_control = get_float_resource(it->dpy, buf,"TVBrightness") / 100.0;
  sprintf(buf,"%sTVContrast",prefix);
  it->contrast_control = get_float_resource(it->dpy, buf,"TVContrast") / 100.0;
  it->height_control = 1.0;
  it->width_control = 1.0;
  it->squish_control = 0.0;
  it->powerup=1000.0;

  it->hashnoise_rpm=0;
  it->hashnoise_on=0;
  it->hashnoise_enable=1;

  it->horiz_desync=frand(10.0)-5.0;
  it->squeezebottom=frand(5.0)-1.0;

#ifdef DEBUG
  printf("analogtv: prefix=%s\n",prefix);
  printf("  use: shm=%d cmap=%d color=%d\n",
         it->use_shm,it->use_cmap,it->use_color);
  printf("  controls: tint=%g color=%g brightness=%g contrast=%g\n",
         it->tint_control, it->color_control, it->brightness_control,
         it->contrast_control);
  printf("  freq_error %g: %g %d\n",
         it->freq_error, it->freq_error_inc, it->flutter_tint);
  printf("  desync: %g %d\n",
         it->horiz_desync, it->flutter_horiz_desync);
  printf("  hashnoise rpm: %g\n",
         it->hashnoise_rpm);
  printf("  vis: %d %d %d\n",
         it->visclass, it->visbits, it->visdepth);
  printf("  shift: %d-%d %d-%d %d-%d\n",
         it->red_invprec,it->red_shift,
         it->green_invprec,it->green_shift,
         it->blue_invprec,it->blue_shift);
  printf("  size: %d %d  %d %d  xrepl=%d\n",
         it->usewidth, it->useheight,
         it->screen_xo, it->screen_yo, it->xrepl);

  printf("    ANALOGTV_V=%d\n",ANALOGTV_V);
  printf("    ANALOGTV_TOP=%d\n",ANALOGTV_TOP);
  printf("    ANALOGTV_VISLINES=%d\n",ANALOGTV_VISLINES);
  printf("    ANALOGTV_BOT=%d\n",ANALOGTV_BOT);
  printf("    ANALOGTV_H=%d\n",ANALOGTV_H);
  printf("    ANALOGTV_SYNC_START=%d\n",ANALOGTV_SYNC_START);
  printf("    ANALOGTV_BP_START=%d\n",ANALOGTV_BP_START);
  printf("    ANALOGTV_CB_START=%d\n",ANALOGTV_CB_START);
  printf("    ANALOGTV_PIC_START=%d\n",ANALOGTV_PIC_START);
  printf("    ANALOGTV_PIC_LEN=%d\n",ANALOGTV_PIC_LEN);
  printf("    ANALOGTV_FP_START=%d\n",ANALOGTV_FP_START);
  printf("    ANALOGTV_PIC_END=%d\n",ANALOGTV_PIC_END);
  printf("    ANALOGTV_HASHNOISE_LEN=%d\n",ANALOGTV_HASHNOISE_LEN);

#endif

}

extern Bool mono_p; /* shoot me */

static void
analogtv_free_image(analogtv *it)
{
  if (it->image) {
    if (it->use_shm) {
#ifdef HAVE_XSHM_EXTENSION
      destroy_xshm_image(it->dpy, it->image, &it->shm_info);
#endif
    } else {
      XDestroyImage(it->image);
    }
    it->image=NULL;
  }
}

static void
analogtv_alloc_image(analogtv *it)
{
  if (it->use_shm) {
#ifdef HAVE_XSHM_EXTENSION
    it->image=create_xshm_image(it->dpy, it->xgwa.visual, it->xgwa.depth, ZPixmap, 0,
                                &it->shm_info, it->usewidth, it->useheight);
#endif
    if (!it->image) it->use_shm=0;
  }
  if (!it->image) {
    it->image = XCreateImage(it->dpy, it->xgwa.visual, it->xgwa.depth, ZPixmap, 0, 0,
                             it->usewidth, it->useheight, 8, 0);
    it->image->data = (char *)malloc(it->image->height * it->image->bytes_per_line);
  }
  memset (it->image->data, 0, it->image->height * it->image->bytes_per_line);
}


static void
analogtv_configure(analogtv *it)
{
  int oldwidth=it->usewidth;
  int oldheight=it->useheight;
  int wlim,hlim,height_diff;

  /* If the window is very small, don't let the image we draw get lower
     than the actual TV resolution (266x200.)

     If the aspect ratio of the window is within 15% of a 4:3 ratio,
     then scale the image to exactly fill the window.

     Otherwise, center the image either horizontally or vertically,
     padding on the left+right, or top+bottom, but not both.

     If it's very close (2.5%) to a multiple of VISLINES, make it exact
     For example, it maps 1024 => 1000.
   */
  float percent = 0.15;  /* jwz: 20% caused severe top/bottom clipping
                                 in Pong on 1680x1050 iMac screen. */
  float min_ratio = 4.0 / 3.0 * (1 - percent);
  float max_ratio = 4.0 / 3.0 * (1 + percent);
  float ratio;
  float height_snap=0.025;

  hlim = it->xgwa.height;
  wlim = it->xgwa.width;
  ratio = wlim / (float) hlim;

  if (wlim < 266 || hlim < 200)
    {
      wlim = 266;
      hlim = 200;
# ifdef DEBUG
      fprintf (stderr,
               "size: minimal: %dx%d in %dx%d (%.3f < %.3f < %.3f)\n",
               wlim, hlim, it->xgwa.width, it->xgwa.height,
               min_ratio, ratio, max_ratio);
# endif
    }
  else if (ratio > min_ratio && ratio < max_ratio)
    {
# ifdef DEBUG
      fprintf (stderr,
               "size: close enough: %dx%d (%.3f < %.3f < %.3f)\n",
               wlim, hlim, min_ratio, ratio, max_ratio);
# endif
    }
  else if (ratio > max_ratio)
    {
      wlim = hlim*max_ratio;
# ifdef DEBUG
      fprintf (stderr,
               "size: center H: %dx%d in %dx%d (%.3f < %.3f < %.3f)\n",
               wlim, hlim, it->xgwa.width, it->xgwa.height,
               min_ratio, ratio, max_ratio);
# endif
    }
  else /* ratio < min_ratio */
    {
      hlim = wlim/min_ratio;
# ifdef DEBUG
      fprintf (stderr,
               "size: center V: %dx%d in %dx%d (%.3f < %.3f < %.3f)\n",
               wlim, hlim, it->xgwa.width, it->xgwa.height,
               min_ratio, ratio, max_ratio);
# endif
    }


  height_diff = ((hlim + ANALOGTV_VISLINES/2) % ANALOGTV_VISLINES) - ANALOGTV_VISLINES/2;
  if (height_diff != 0 && fabs(height_diff) < hlim * height_snap)
    {
      hlim -= height_diff;
    }


  /* Most times this doesn't change */
  if (wlim != oldwidth || hlim != oldheight) {

    it->usewidth=wlim;
    it->useheight=hlim;

    it->xrepl=1+it->usewidth/640;
    if (it->xrepl>2) it->xrepl=2;
    it->subwidth=it->usewidth/it->xrepl;

    analogtv_free_image(it);
    analogtv_alloc_image(it);
  }

  it->screen_xo = (it->xgwa.width-it->usewidth)/2;
  it->screen_yo = (it->xgwa.height-it->useheight)/2;
  it->need_clear=1;
}

void
analogtv_reconfigure(analogtv *it)
{
  XGetWindowAttributes (it->dpy, it->window, &it->xgwa);
  analogtv_configure(it);
}

analogtv *
analogtv_allocate(Display *dpy, Window window)
{
  XGCValues gcv;
  analogtv *it=NULL;
  int i;

  analogtv_init();

  it=(analogtv *)calloc(1,sizeof(analogtv));
  if (!it) return 0;
  it->dpy=dpy;
  it->window=window;

  it->shrinkpulse=-1;

  it->n_colors=0;

#ifdef HAVE_XSHM_EXTENSION
  it->use_shm=1;
#else
  it->use_shm=0;
#endif

  XGetWindowAttributes (it->dpy, it->window, &it->xgwa);

  it->screen=it->xgwa.screen;
  it->colormap=it->xgwa.colormap;
  it->visclass=it->xgwa.visual->class;
  it->visbits=it->xgwa.visual->bits_per_rgb;
  it->visdepth=it->xgwa.depth;
  if (it->visclass == TrueColor || it->visclass == DirectColor) {
    if (get_integer_resource (it->dpy, "use_cmap", "Integer")) {
      it->use_cmap=1;
    } else {
      it->use_cmap=0;
    }
    it->use_color=!mono_p;
  }
  else if (it->visclass == PseudoColor || it->visclass == StaticColor) {
    it->use_cmap=1;
    it->use_color=!mono_p;
  }
  else {
    it->use_cmap=1;
    it->use_color=0;
  }

  it->red_mask=it->xgwa.visual->red_mask;
  it->green_mask=it->xgwa.visual->green_mask;
  it->blue_mask=it->xgwa.visual->blue_mask;
  it->red_shift=it->red_invprec=-1;
  it->green_shift=it->green_invprec=-1;
  it->blue_shift=it->blue_invprec=-1;
  if (!it->use_cmap) {
    /* Is there a standard way to do this? Does this handle all cases? */
    int shift, prec;
    for (shift=0; shift<32; shift++) {
      for (prec=1; prec<16 && prec<40-shift; prec++) {
        unsigned long mask=(0xffffUL>>(16-prec)) << shift;
        if (it->red_shift<0 && mask==it->red_mask)
          it->red_shift=shift, it->red_invprec=16-prec;
        if (it->green_shift<0 && mask==it->green_mask)
          it->green_shift=shift, it->green_invprec=16-prec;
        if (it->blue_shift<0 && mask==it->blue_mask)
          it->blue_shift=shift, it->blue_invprec=16-prec;
      }
    }
    if (it->red_shift<0 || it->green_shift<0 || it->blue_shift<0) {
      if (0) fprintf(stderr,"Can't figure out color space\n");
      goto fail;
    }

    for (i=0; i<ANALOGTV_CV_MAX; i++) {
      int intensity=pow(i/256.0, 0.8)*65535.0; /* gamma correction */
      if (intensity>65535) intensity=65535;
      it->red_values[i]=((intensity>>it->red_invprec)<<it->red_shift);
      it->green_values[i]=((intensity>>it->green_invprec)<<it->green_shift);
      it->blue_values[i]=((intensity>>it->blue_invprec)<<it->blue_shift);
    }

  }

  gcv.background=get_pixel_resource(it->dpy, it->colormap,
                                    "background", "Background");

  it->gc = XCreateGC(it->dpy, it->window, GCBackground, &gcv);
  XSetWindowBackground(it->dpy, it->window, gcv.background);
  XClearWindow(dpy,window);

  analogtv_configure(it);

  return it;

 fail:
  if (it) free(it);
  return NULL;
}

void
analogtv_release(analogtv *it)
{
  if (it->image) {
    if (it->use_shm) {
#ifdef HAVE_XSHM_EXTENSION
      destroy_xshm_image(it->dpy, it->image, &it->shm_info);
#endif
    } else {
      XDestroyImage(it->image);
    }
    it->image=NULL;
  }
  if (it->gc) XFreeGC(it->dpy, it->gc);
  it->gc=NULL;
  if (it->n_colors) XFreeColors(it->dpy, it->colormap, it->colors, it->n_colors, 0L);
  it->n_colors=0;
  free(it);
}


/*
  First generate the I and Q reference signals, which we'll multiply
  the input signal by to accomplish the demodulation. Normally they
  are shifted 33 degrees from the colorburst. I think this was convenient
  for inductor-capacitor-vacuum tube implementation.

  The tint control, FWIW, just adds a phase shift to the chroma signal,
  and the color control controls the amplitude.

  In text modes (colormode==0) the system disabled the color burst, and no
  color was detected by the monitor.

  freq_error gives a mismatch between the built-in oscillator and the
  TV's colorbust. Some II Plus machines seemed to occasionally get
  instability problems -- the crystal oscillator was a single
  transistor if I remember correctly -- and the frequency would vary
  enough that the tint would change across the width of the screen.
  The left side would be in correct tint because it had just gotten
  resynchronized with the color burst.

  If we're using a colormap, set it up.
*/
int
analogtv_set_demod(analogtv *it)
{
  int y_levels=10,i_levels=5,q_levels=5;

  /*
    In principle, we might be able to figure out how to adjust the
    color map frame-by-frame to get some nice color bummage. But I'm
    terrified of changing the color map because we'll get flashing.

    I can hardly believe we still have to deal with colormaps. They're
    like having NEAR PTRs: an enormous hassle for the programmer just
    to save on memory. They should have been deprecated by 1995 or
    so. */

 cmap_again:
  if (it->use_cmap && !it->n_colors) {

    if (it->n_colors) {
      XFreeColors(it->dpy, it->colormap, it->colors, it->n_colors, 0L);
      it->n_colors=0;
    }

    {
      int yli,qli,ili;
      for (yli=0; yli<y_levels; yli++) {
        for (ili=0; ili<i_levels; ili++) {
          for (qli=0; qli<q_levels; qli++) {
            double interpy,interpi,interpq;
            double levelmult=700.0;
            int r,g,b;
            XColor col;

            interpy=100.0 * ((double)yli/y_levels);
            interpi=50.0 * (((double)ili-(0.5*i_levels))/(double)i_levels);
            interpq=50.0 * (((double)qli-(0.5*q_levels))/(double)q_levels);

            r=(int)((interpy + 1.04*interpi + 0.624*interpq)*levelmult);
            g=(int)((interpy - 0.276*interpi - 0.639*interpq)*levelmult);
            b=(int)((interpy - 1.105*interpi + 1.729*interpq)*levelmult);
            if (r<0) r=0;
            if (r>65535) r=65535;
            if (g<0) g=0;
            if (g>65535) g=65535;
            if (b<0) b=0;
            if (b>65535) b=65535;

#ifdef DEBUG
            printf("%0.2f %0.2f %0.2f => %02x%02x%02x\n",
                   interpy, interpi, interpq,
                   r/256,g/256,b/256);
#endif

            col.red=r;
            col.green=g;
            col.blue=b;
            col.pixel=0;
            if (!XAllocColor(it->dpy, it->colormap, &col)) {
              if (q_levels > y_levels*4/12)
                q_levels--;
              else if (i_levels > y_levels*5/12)
                i_levels--;
              else
                y_levels--;

              if (y_levels<2)
                return -1;
              goto cmap_again;
            }
            it->colors[it->n_colors++]=col.pixel;
          }
        }
      }

      it->cmap_y_levels=y_levels;
      it->cmap_i_levels=i_levels;
      it->cmap_q_levels=q_levels;
    }
  }

  return 0;
}

#if 0
unsigned int
analogtv_line_signature(analogtv_input *input, int lineno)
{
  int i;
  char *origsignal=&input->signal[(lineno+input->vsync)
                                  %ANALOGTV_V][input->line_hsync[lineno]];
  unsigned int hash=0;

  /* probably lame */
  for (i=0; i<ANALOGTV_PIC_LEN; i++) {
    int c=origsignal[i];
    hash = hash + (hash<<17) + c;
  }

  hash += input->line_hsync[lineno];
  hash ^= hash >> 2;
  /*
  hash += input->hashnoise_times[lineno];
  hash ^= hash >> 2;
  */

  return hash;
}
#endif


/* Here we model the analog circuitry of an NTSC television.
   Basically, it splits the signal into 3 signals: Y, I and Q. Y
   corresponds to luminance, and you get it by low-pass filtering the
   input signal to below 3.57 MHz.

   I and Q are the in-phase and quadrature components of the 3.57 MHz
   subcarrier. We get them by multiplying by cos(3.57 MHz*t) and
   sin(3.57 MHz*t), and low-pass filtering. Because the eye has less
   resolution in some colors than others, the I component gets
   low-pass filtered at 1.5 MHz and the Q at 0.5 MHz. The I component
   is approximately orange-blue, and Q is roughly purple-green. See
   http://www.ntsc-tv.com for details.

   We actually do an awful lot to the signal here. I suspect it would
   make sense to wrap them all up together by calculating impulse
   response and doing FFT convolutions.

*/

static void
analogtv_ntsc_to_yiq(analogtv *it, int lineno, double *signal,
                     int start, int end)
{
  enum {MAXDELAY=32};
  int i;
  double *sp;
  int phasecorr=(signal-it->rx_signal)&3;
  struct analogtv_yiq_s *yiq;
  int colormode;
  double agclevel=it->agclevel;
  double brightadd=it->brightness_control*100.0 - ANALOGTV_BLACK_LEVEL;
  double delay[MAXDELAY+ANALOGTV_PIC_LEN], *dp;
  double multiq2[4];

  {

    double cb_i=(it->line_cb_phase[lineno][(2+phasecorr)&3]-
                 it->line_cb_phase[lineno][(0+phasecorr)&3])/16.0;
    double cb_q=(it->line_cb_phase[lineno][(3+phasecorr)&3]-
                 it->line_cb_phase[lineno][(1+phasecorr)&3])/16.0;

    colormode = (cb_i * cb_i + cb_q * cb_q) > 2.8;

    if (colormode) {
      double tint_i = -cos((103 + it->color_control)*3.1415926/180);
      double tint_q = sin((103 + it->color_control)*3.1415926/180);

      multiq2[0] = (cb_i*tint_i - cb_q*tint_q) * it->color_control;
      multiq2[1] = (cb_q*tint_i + cb_i*tint_q) * it->color_control;
      multiq2[2]=-multiq2[0];
      multiq2[3]=-multiq2[1];
    }
  }

#if 0
  if (lineno==100) {
    printf("multiq = [%0.3f %0.3f %0.3f %0.3f] ",
           it->multiq[60],it->multiq[61],it->multiq[62],it->multiq[63]);
    printf("it->line_cb_phase = [%0.3f %0.3f %0.3f %0.3f]\n",
           it->line_cb_phase[lineno][0],it->line_cb_phase[lineno][1],
           it->line_cb_phase[lineno][2],it->line_cb_phase[lineno][3]);
    printf("multiq2 = [%0.3f %0.3f %0.3f %0.3f]\n",
           multiq2[0],multiq2[1],multiq2[2],multiq2[3]);
  }
#endif

  dp=delay+ANALOGTV_PIC_LEN-MAXDELAY;
  for (i=0; i<5; i++) dp[i]=0.0;

  assert(start>=0);
  assert(end < ANALOGTV_PIC_LEN+10);

  dp=delay+ANALOGTV_PIC_LEN-MAXDELAY;
  for (i=0; i<24; i++) dp[i]=0.0;
  for (i=start, yiq=it->yiq+start, sp=signal+start;
       i<end;
       i++, dp--, yiq++, sp++) {

    /* Now filter them. These are infinite impulse response filters
       calculated by the script at
       http://www-users.cs.york.ac.uk/~fisher/mkfilter. This is
       fixed-point integer DSP, son. No place for wimps. We do it in
       integer because you can count on integer being faster on most
       CPUs. We care about speed because we need to recalculate every
       time we blink text, and when we spew random bytes into screen
       memory. This is roughly 16.16 fixed point arithmetic, but we
       scale some filter values up by a few bits to avoid some nasty
       precision errors. */

    /* Filter Y with a 4-pole low-pass Butterworth filter at 3.5 MHz
       with an extra zero at 3.5 MHz, from
       mkfilter -Bu -Lp -o 4 -a 2.1428571429e-01 0 -Z 2.5e-01 -l
       Delay about 2 */

    dp[0] = sp[0] * 0.0469904257251935 * agclevel;
    dp[8] = (+1.0*(dp[6]+dp[0])
             +4.0*(dp[5]+dp[1])
             +7.0*(dp[4]+dp[2])
             +8.0*(dp[3])
             -0.0176648*dp[12]
             -0.4860288*dp[10]);
    yiq->y = dp[8] + brightadd;
  }

  if (colormode) {
    dp=delay+ANALOGTV_PIC_LEN-MAXDELAY;
    for (i=0; i<27; i++) dp[i]=0.0;

    for (i=start, yiq=it->yiq+start, sp=signal+start;
         i<end;
         i++, dp--, yiq++, sp++) {
      double sig=*sp;

      /* Filter I and Q with a 3-pole low-pass Butterworth filter at
         1.5 MHz with an extra zero at 3.5 MHz, from
         mkfilter -Bu -Lp -o 3 -a 1.0714285714e-01 0 -Z 2.5000000000e-01 -l
         Delay about 3.
      */

      dp[0] = sig*multiq2[i&3] * 0.0833333333333;
      yiq->i=dp[8] = (dp[5] + dp[0]
                      +3.0*(dp[4] + dp[1])
                      +4.0*(dp[3] + dp[2])
                      -0.3333333333 * dp[10]);

      dp[16] = sig*multiq2[(i+3)&3] * 0.0833333333333;
      yiq->q=dp[24] = (dp[16+5] + dp[16+0]
                       +3.0*(dp[16+4] + dp[16+1])
                       +4.0*(dp[16+3] + dp[16+2])
                       -0.3333333333 * dp[24+2]);
    }
  } else {
    for (i=start, yiq=it->yiq+start; i<end; i++, yiq++) {
      yiq->i = yiq->q = 0.0;
    }
  }
}

void
analogtv_setup_teletext(analogtv_input *input)
{
  int x,y;
  int teletext=ANALOGTV_BLACK_LEVEL;

  /* Teletext goes in line 21. But I suspect there are other things
     in the vertical retrace interval */

  for (y=19; y<22; y++) {
    for (x=ANALOGTV_PIC_START; x<ANALOGTV_PIC_END; x++) {
      if ((x&7)==0) {
        teletext=(random()&1) ? ANALOGTV_WHITE_LEVEL : ANALOGTV_BLACK_LEVEL;
      }
      input->signal[y][x]=teletext;
    }
  }
}

void
analogtv_setup_frame(analogtv *it)
{
  int i,x,y;

  it->redraw_all=0;

  if (it->flutter_horiz_desync) {
    /* Horizontal sync during vertical sync instability. */
    it->horiz_desync += -0.10*(it->horiz_desync-3.0) +
      ((int)(random()&0xff)-0x80) *
      ((int)(random()&0xff)-0x80) *
      ((int)(random()&0xff)-0x80) * 0.000001;
  }

  for (i=0; i<ANALOGTV_V; i++) {
    it->hashnoise_times[i]=0;
  }

  if (it->hashnoise_enable && !it->hashnoise_on) {
    if (random()%10000==0) {
      it->hashnoise_on=1;
      it->shrinkpulse=random()%ANALOGTV_V;
    }
  }
  if (random()%1000==0) {
    it->hashnoise_on=0;
  }
  if (it->hashnoise_on) {
    it->hashnoise_rpm += (15000.0 - it->hashnoise_rpm)*0.05 +
      ((int)(random()%2000)-1000)*0.1;
  } else {
    it->hashnoise_rpm -= 100 + 0.01*it->hashnoise_rpm;
    if (it->hashnoise_rpm<0.0) it->hashnoise_rpm=0.0;
  }
  if (it->hashnoise_rpm > 0.0) {
    int hni;
    int hnc=it->hashnoise_counter; /* in 24.8 format */

    /* Convert rpm of a 16-pole motor into dots in 24.8 format */
    hni = (int)(ANALOGTV_V * ANALOGTV_H * 256.0 /
                (it->hashnoise_rpm * 16.0 / 60.0 / 60.0));

    while (hnc < (ANALOGTV_V * ANALOGTV_H)<<8) {
      y=(hnc>>8)/ANALOGTV_H;
      x=(hnc>>8)%ANALOGTV_H;

      if (x>0 && x<ANALOGTV_H - ANALOGTV_HASHNOISE_LEN) {
        it->hashnoise_times[y]=x;
      }
      hnc += hni + (int)(random()%65536)-32768;
    }
    hnc -= (ANALOGTV_V * ANALOGTV_H)<<8;
  }

  if (it->rx_signal_level != 0.0)
    it->agclevel = 1.0/it->rx_signal_level;


#ifdef DEBUG2
  printf("filter: ");
  for (i=0; i<ANALOGTV_GHOSTFIR_LEN; i++) {
    printf(" %0.3f",it->ghostfir[i]);
  }
  printf(" siglevel=%g agc=%g\n", siglevel, it->agclevel);
#endif
}

void
analogtv_setup_sync(analogtv_input *input, int do_cb, int do_ssavi)
{
  int i,lineno,vsync;
  signed char *sig;

  int synclevel = do_ssavi ? ANALOGTV_WHITE_LEVEL : ANALOGTV_SYNC_LEVEL;

  for (lineno=0; lineno<ANALOGTV_V; lineno++) {
    vsync=lineno>=3 && lineno<7;

    sig=input->signal[lineno];

    i=ANALOGTV_SYNC_START;
    if (vsync) {
      while (i<ANALOGTV_BP_START) sig[i++]=ANALOGTV_BLANK_LEVEL;
      while (i<ANALOGTV_H) sig[i++]=synclevel;
    } else {
      while (i<ANALOGTV_BP_START) sig[i++]=synclevel;
      while (i<ANALOGTV_PIC_START) sig[i++]=ANALOGTV_BLANK_LEVEL;
      while (i<ANALOGTV_FP_START) sig[i++]=ANALOGTV_BLACK_LEVEL;
    }
    while (i<ANALOGTV_H) sig[i++]=ANALOGTV_BLANK_LEVEL;

    if (do_cb) {
      /* 9 cycles of colorburst */
      for (i=ANALOGTV_CB_START; i<ANALOGTV_CB_START+36; i+=4) {
        sig[i+1] += ANALOGTV_CB_LEVEL;
        sig[i+3] -= ANALOGTV_CB_LEVEL;
      }
    }
  }
}

static void
analogtv_sync(analogtv *it)
{
  int cur_hsync=it->cur_hsync;
  int cur_vsync=it->cur_vsync;
  int lineno = 0;
  int i,j;
  double osc,filt;
  double *sp;
  double cbfc=1.0/128.0;

  sp = it->rx_signal + lineno*ANALOGTV_H + cur_hsync;
  for (i=-32; i<32; i++) {
    lineno = (cur_vsync + i + ANALOGTV_V) % ANALOGTV_V;
    sp = it->rx_signal + lineno*ANALOGTV_H;
    filt=0.0;
    for (j=0; j<ANALOGTV_H; j+=ANALOGTV_H/16) {
      filt += sp[j];
    }
    filt *= it->agclevel;

    osc = (double)(ANALOGTV_V+i)/(double)ANALOGTV_V;

    if (osc >= 1.05+0.0002 * filt) break;
  }
  cur_vsync = (cur_vsync + i + ANALOGTV_V) % ANALOGTV_V;

  for (lineno=0; lineno<ANALOGTV_V; lineno++) {

    if (lineno>5 && lineno<ANALOGTV_V-3) { /* ignore vsync interval */

      sp = it->rx_signal + ((lineno + cur_vsync + ANALOGTV_V)%ANALOGTV_V
                            )*ANALOGTV_H + cur_hsync;
      for (i=-8; i<8; i++) {
        osc = (double)(ANALOGTV_H+i)/(double)ANALOGTV_H;
        filt=(sp[i-3]+sp[i-2]+sp[i-1]+sp[i]) * it->agclevel;

        if (osc >= 1.005 + 0.0001*filt) break;
      }
      cur_hsync = (cur_hsync + i + ANALOGTV_H) % ANALOGTV_H;
    }

    it->line_hsync[lineno]=(cur_hsync + ANALOGTV_PIC_START +
                            ANALOGTV_H) % ANALOGTV_H;

    /* Now look for the colorburst, which is a few cycles after the H
       sync pulse, and store its phase.
       The colorburst is 9 cycles long, and we look at the middle 5
       cycles.
    */

    if (lineno>15) {
      sp = it->rx_signal + lineno*ANALOGTV_H + (cur_hsync&~3);
      for (i=ANALOGTV_CB_START+8; i<ANALOGTV_CB_START+36-8; i++) {
        it->cb_phase[i&3] = it->cb_phase[i&3]*(1.0-cbfc) +
          sp[i]*it->agclevel*cbfc;
      }
    }

    {
      double tot=0.1;
      double cbgain;

      for (i=0; i<4; i++) {
        tot += it->cb_phase[i]*it->cb_phase[i];
      }
      cbgain = 32.0/sqrt(tot);

      for (i=0; i<4; i++) {
        it->line_cb_phase[lineno][i]=it->cb_phase[i]*cbgain;
      }
    }

#ifdef DEBUG
    if (0) printf("hs=%d cb=[%0.3f %0.3f %0.3f %0.3f]\n",
                  cur_hsync,
                  it->cb_phase[0], it->cb_phase[1],
                  it->cb_phase[2], it->cb_phase[3]);
#endif

    /* if (random()%2000==0) cur_hsync=random()%ANALOGTV_H; */
  }

  it->cur_hsync = cur_hsync;
  it->cur_vsync = cur_vsync;
}

static double
analogtv_levelmult(analogtv *it, int level)
{
  static const double levelfac[3]={-7.5, 5.5, 24.5};
  return (40.0 + levelfac[level]*puramp(it, 3.0, 6.0, 1.0))/256.0;
}

static int
analogtv_level(analogtv *it, int y, int ytop, int ybot)
{
  int level;
  if (ybot-ytop>=7) {
    if (y==ytop || y==ybot-1) level=0;
    else if (y==ytop+1 || y==ybot-2) level=1;
    else level=2;
  }
  else if (ybot-ytop>=5) {
    if (y==ytop || y==ybot-1) level=0;
    else level=2;
  }
  else if (ybot-ytop>=3) {
    if (y==ytop) level=0;
    else level=2;
  }
  else {
    level=2;
  }
  return level;
}

/*
  The point of this stuff is to ensure that when useheight is not a
  multiple of VISLINES so that TV scan lines map to different numbers
  of vertical screen pixels, the total brightness of each scan line
  remains the same.
  ANALOGTV_MAX_LINEHEIGHT corresponds to 2400 vertical pixels, beyond which
  it interpolates extra black lines.
 */

static void
analogtv_setup_levels(analogtv *it, double avgheight)
{
  int i,height;
  static const double levelfac[3]={-7.5, 5.5, 24.5};

  for (height=0; height<avgheight+2.0 && height<=ANALOGTV_MAX_LINEHEIGHT; height++) {

    for (i=0; i<height; i++) {
      it->leveltable[height][i].index = 2;
    }
    
    if (avgheight>=3) {
      it->leveltable[height][0].index=0;
    }
    if (avgheight>=5) {
      if (height >= 1) it->leveltable[height][height-1].index=0;
    }
    if (avgheight>=7) {
      it->leveltable[height][1].index=1;
      if (height >= 2) it->leveltable[height][height-2].index=1;
    }

    for (i=0; i<height; i++) {
      it->leveltable[height][i].value = 
        (40.0 + levelfac[it->leveltable[height][i].index]*puramp(it, 3.0, 6.0, 1.0)) / 256.0;
    }

  }
}

static void
analogtv_blast_imagerow(analogtv *it,
                        float *rgbf, float *rgbf_end,
                        int ytop, int ybot)
{
  int i,j,x,y;
  float *rpf;
  char *level_copyfrom[3];
  int xrepl=it->xrepl;
  for (i=0; i<3; i++) level_copyfrom[i]=NULL;

  for (y=ytop; y<ybot; y++) {
    int level=it->leveltable[ybot-ytop][y-ytop].index;
    double levelmult=it->leveltable[ybot-ytop][y-ytop].value;
    char *rowdata;

    rowdata=it->image->data + y*it->image->bytes_per_line;

    /* Fast special cases to avoid the slow XPutPixel. Ugh. It goes to show
       why standard graphics sw has to be fast, or else people will have to
       work around it and risk incompatibility. The quickdraw folks
       understood this. The other answer would be for X11 to have fewer
       formats for bitm.. oh, never mind. If neither of these cases work
       (they probably cover 99% of setups) it falls back on the Xlib
       routines. */

    if (level_copyfrom[level]) {
      memcpy(rowdata, level_copyfrom[level], it->image->bytes_per_line);
    }
    else {
      level_copyfrom[level] = rowdata;

      if (0) {
      }
      else if (it->image->format==ZPixmap &&
               it->image->bits_per_pixel==32 &&
               sizeof(unsigned int)==4 &&
               it->image->byte_order==localbyteorder) {
        /* int is more likely to be 32 bits than long */
        unsigned int *pixelptr=(unsigned int *)rowdata;
        unsigned int pix;

        for (rpf=rgbf; rpf!=rgbf_end; rpf+=3) {
          int ntscri=rpf[0]*levelmult;
          int ntscgi=rpf[1]*levelmult;
          int ntscbi=rpf[2]*levelmult;
          if (ntscri>=ANALOGTV_CV_MAX) ntscri=ANALOGTV_CV_MAX-1;
          if (ntscgi>=ANALOGTV_CV_MAX) ntscgi=ANALOGTV_CV_MAX-1;
          if (ntscbi>=ANALOGTV_CV_MAX) ntscbi=ANALOGTV_CV_MAX-1;
          pix = (it->red_values[ntscri] |
                 it->green_values[ntscgi] |
                 it->blue_values[ntscbi]);
          pixelptr[0] = pix;
          if (xrepl>=2) {
            pixelptr[1] = pix;
            if (xrepl>=3) pixelptr[2] = pix;
          }
          pixelptr+=xrepl;
        }
      }
      else if (it->image->format==ZPixmap &&
               it->image->bits_per_pixel==16 &&
               sizeof(unsigned short)==2 &&
               float_extraction_works &&
               it->image->byte_order==localbyteorder) {
        unsigned short *pixelptr=(unsigned short *)rowdata;
        double r2,g2,b2;
        float_extract_t r1,g1,b1;
        unsigned short pix;

        for (rpf=rgbf; rpf!=rgbf_end; rpf+=3) {
          r2=rpf[0]; g2=rpf[1]; b2=rpf[2];
          r1.f=r2 * levelmult+float_low8_ofs;
          g1.f=g2 * levelmult+float_low8_ofs;
          b1.f=b2 * levelmult+float_low8_ofs;
          pix = (it->red_values[r1.i & 0x3ff] |
                 it->green_values[g1.i & 0x3ff] |
                 it->blue_values[b1.i & 0x3ff]);
          pixelptr[0] = pix;
          if (xrepl>=2) {
            pixelptr[1] = pix;
            if (xrepl>=3) pixelptr[2] = pix;
          }
          pixelptr+=xrepl;
        }
      }
      else if (it->image->format==ZPixmap &&
               it->image->bits_per_pixel==16 &&
               sizeof(unsigned short)==2 &&
               it->image->byte_order==localbyteorder) {
        unsigned short *pixelptr=(unsigned short *)rowdata;
        unsigned short pix;

        for (rpf=rgbf; rpf!=rgbf_end; rpf+=3) {
          int r1=rpf[0] * levelmult;
          int g1=rpf[1] * levelmult;
          int b1=rpf[2] * levelmult;
          if (r1>=ANALOGTV_CV_MAX) r1=ANALOGTV_CV_MAX-1;
          if (g1>=ANALOGTV_CV_MAX) g1=ANALOGTV_CV_MAX-1;
          if (b1>=ANALOGTV_CV_MAX) b1=ANALOGTV_CV_MAX-1;
          pix = it->red_values[r1] | it->green_values[g1] | it->blue_values[b1];
          pixelptr[0] = pix;
          if (xrepl>=2) {
            pixelptr[1] = pix;
            if (xrepl>=3) pixelptr[2] = pix;
          }
          pixelptr+=xrepl;
        }
      }
      else {
        for (x=0, rpf=rgbf; rpf!=rgbf_end ; x++, rpf+=3) {
          int ntscri=rpf[0]*levelmult;
          int ntscgi=rpf[1]*levelmult;
          int ntscbi=rpf[2]*levelmult;
          if (ntscri>=ANALOGTV_CV_MAX) ntscri=ANALOGTV_CV_MAX-1;
          if (ntscgi>=ANALOGTV_CV_MAX) ntscgi=ANALOGTV_CV_MAX-1;
          if (ntscbi>=ANALOGTV_CV_MAX) ntscbi=ANALOGTV_CV_MAX-1;
          for (j=0; j<xrepl; j++) {
            XPutPixel(it->image, x*xrepl + j, y,
                      it->red_values[ntscri] | it->green_values[ntscgi] |
                      it->blue_values[ntscbi]);
          }
        }
      }
    }
  }
}

void
analogtv_draw(analogtv *it)
{
  int i,j,x,y,lineno;
  int scanstart_i,scanend_i,squishright_i,squishdiv,pixrate;
  float *rgb_start, *rgb_end;
  double pixbright;
  int pixmultinc;
  int bigloadchange,drawcount;
  double baseload;
  double puheight;
  int overall_top, overall_bot;

  float *raw_rgb_start=(float *)calloc(it->subwidth*3, sizeof(float));
  float *raw_rgb_end=raw_rgb_start+3*it->subwidth;
  float *rrp;

  if (! raw_rgb_start) return;
  analogtv_setup_frame(it);
  analogtv_set_demod(it);

  /* rx_signal has an extra 2 lines at the end, where we copy the
     first 2 lines so we can index into it while only worrying about
     wraparound on a per-line level */
  memcpy(&it->rx_signal[ANALOGTV_SIGNAL_LEN],
         &it->rx_signal[0],
         2*ANALOGTV_H*sizeof(it->rx_signal[0]));

  analogtv_sync(it);

  baseload=0.5;
  /* if (it->hashnoise_on) baseload=0.5; */

  bigloadchange=1;
  drawcount=0;
  it->crtload[ANALOGTV_TOP-1]=baseload;
  puheight = puramp(it, 2.0, 1.0, 1.3) * it->height_control *
    (1.125 - 0.125*puramp(it, 2.0, 2.0, 1.1));

  analogtv_setup_levels(it, puheight * (double)it->useheight/(double)ANALOGTV_VISLINES);

  overall_top=it->useheight;
  overall_bot=0;

  for (lineno=ANALOGTV_TOP; lineno<ANALOGTV_BOT; lineno++) {
    int slineno=lineno-ANALOGTV_TOP;
    int ytop=(int)((slineno*it->useheight/ANALOGTV_VISLINES -
                    it->useheight/2)*puheight) + it->useheight/2;
    int ybot=(int)(((slineno+1)*it->useheight/ANALOGTV_VISLINES -
                    it->useheight/2)*puheight) + it->useheight/2;
#if 0
    int linesig=analogtv_line_signature(input,lineno)
      + it->hashnoise_times[lineno];
#endif
    double *signal=(it->rx_signal + ((lineno + it->cur_vsync +
                                      ANALOGTV_V)%ANALOGTV_V) * ANALOGTV_H +
                    it->line_hsync[lineno]);

    if (ytop==ybot) continue;
    if (ybot<0 || ytop>it->useheight) continue;
    if (ytop<0) ytop=0;
    if (ybot>it->useheight) ybot=it->useheight;

    if (ybot > ytop+ANALOGTV_MAX_LINEHEIGHT) ybot=ytop+ANALOGTV_MAX_LINEHEIGHT;

    if (ytop < overall_top) overall_top=ytop;
    if (ybot > overall_bot) overall_bot=ybot;

    if (lineno==it->shrinkpulse) {
      baseload += 0.4;
      bigloadchange=1;
      it->shrinkpulse=-1;
    }

#if 0
    if (it->hashnoise_rpm>0.0 &&
        !(bigloadchange ||
          it->redraw_all ||
          (slineno<20 && it->flutter_horiz_desync) ||
          it->gaussiannoise_level>30 ||
          ((it->gaussiannoise_level>2.0 ||
            it->multipath) && random()%4) ||
          linesig != it->onscreen_signature[lineno])) {
      continue;
    }
    it->onscreen_signature[lineno] = linesig;
#endif
    drawcount++;

    /*
      Interpolate the 600-dotclock line into however many horizontal
      screen pixels we're using, and convert to RGB.

      We add some 'bloom', variations in the horizontal scan width with
      the amount of brightness, extremely common on period TV sets. They
      had a single oscillator which generated both the horizontal scan and
      (during the horizontal retrace interval) the high voltage for the
      electron beam. More brightness meant more load on the oscillator,
      which caused an decrease in horizontal deflection. Look for
      (bloomthisrow).

      Also, the A2 did a bad job of generating horizontal sync pulses
      during the vertical blanking interval. This, and the fact that the
      horizontal frequency was a bit off meant that TVs usually went a bit
      out of sync during the vertical retrace, and the top of the screen
      would be bent a bit to the left or right. Look for (shiftthisrow).

      We also add a teeny bit of left overscan, just enough to be
      annoying, but you can still read the left column of text.

      We also simulate compression & brightening on the right side of the
      screen. Most TVs do this, but you don't notice because they overscan
      so it's off the right edge of the CRT. But the A2 video system used
      so much of the horizontal scan line that you had to crank the
      horizontal width down in order to not lose the right few characters,
      and you'd see the compression on the right edge. Associated with
      compression is brightening; since the electron beam was scanning
      slower, the same drive signal hit the phosphor harder. Look for
      (squishright_i) and (squishdiv).
    */

    {
      int totsignal=0;
      double ncl,diff;

      for (i=0; i<ANALOGTV_PIC_LEN; i++) {
        totsignal += signal[i];
      }
      totsignal *= it->agclevel;
      ncl = 0.95 * it->crtload[lineno-1] +
        0.05*(baseload +
              (totsignal-30000)/100000.0 +
              (slineno>184 ? (slineno-184)*(lineno-184)*0.001 * it->squeezebottom
               : 0.0));
      diff=ncl - it->crtload[lineno];
      bigloadchange = (diff>0.01 || diff<-0.01);
      it->crtload[lineno]=ncl;
    }

    {
      double bloomthisrow,shiftthisrow;
      double viswidth,middle;
      double scanwidth;
      int scw,scl,scr;

      bloomthisrow = -10.0 * it->crtload[lineno];
      if (bloomthisrow<-10.0) bloomthisrow=-10.0;
      if (bloomthisrow>2.0) bloomthisrow=2.0;
      if (slineno<16) {
        shiftthisrow=it->horiz_desync * (exp(-0.17*slineno) *
                                         (0.7+cos(slineno*0.6)));
      } else {
        shiftthisrow=0.0;
      }

      viswidth=ANALOGTV_PIC_LEN * 0.79 - 5.0*bloomthisrow;
      middle=ANALOGTV_PIC_LEN/2 - shiftthisrow;

      scanwidth=it->width_control * puramp(it, 0.5, 0.3, 1.0);

      scw=it->subwidth*scanwidth;
      if (scw>it->subwidth) scw=it->usewidth;
      scl=it->subwidth/2 - scw/2;
      scr=it->subwidth/2 + scw/2;

      pixrate=(int)((viswidth*65536.0*1.0)/it->subwidth)/scanwidth;
      scanstart_i=(int)((middle-viswidth*0.5)*65536.0);
      scanend_i=(ANALOGTV_PIC_LEN-1)*65536;
      squishright_i=(int)((middle+viswidth*(0.25 + 0.25*puramp(it, 2.0, 0.0, 1.1)
                                            - it->squish_control)) *65536.0);
      squishdiv=it->subwidth/15;

      rgb_start=raw_rgb_start+scl*3;
      rgb_end=raw_rgb_start+scr*3;

      assert(scanstart_i>=0);

#ifdef DEBUG
      if (0) printf("scan %d: %0.3f %0.3f %0.3f scl=%d scr=%d scw=%d\n",
                    lineno,
                    scanstart_i/65536.0,
                    squishright_i/65536.0,
                    scanend_i/65536.0,
                    scl,scr,scw);
#endif
    }

    if (it->use_cmap) {
      for (y=ytop; y<ybot; y++) {
        int level=analogtv_level(it, y, ytop, ybot);
        double levelmult=analogtv_levelmult(it, level);
        double levelmult_y = levelmult * it->contrast_control
          * puramp(it, 1.0, 0.0, 1.0) / (0.5+0.5*puheight) * 0.070;
        double levelmult_iq = levelmult * 0.090;

        struct analogtv_yiq_s *yiq=it->yiq;
        analogtv_ntsc_to_yiq(it, lineno, signal,
                             (scanstart_i>>16)-10, (scanend_i>>16)+10);
        pixmultinc=pixrate;

        x=0;
        i=scanstart_i;
        while (i<0 && x<it->usewidth) {
          XPutPixel(it->image, x, y, it->colors[0]);
          i+=pixmultinc;
          x++;
        }

        while (i<scanend_i && x<it->usewidth) {
          double pixfrac=(i&0xffff)/65536.0;
          double invpixfrac=(1.0-pixfrac);
          int pati=i>>16;
          int yli,ili,qli,cmi;

          double interpy=(yiq[pati].y*invpixfrac
                          + yiq[pati+1].y*pixfrac) * levelmult_y;
          double interpi=(yiq[pati].i*invpixfrac
                          + yiq[pati+1].i*pixfrac) * levelmult_iq;
          double interpq=(yiq[pati].q*invpixfrac
                          + yiq[pati+1].q*pixfrac) * levelmult_iq;

          yli = (int)(interpy * it->cmap_y_levels);
          ili = (int)((interpi+0.5) * it->cmap_i_levels);
          qli = (int)((interpq+0.5) * it->cmap_q_levels);
          if (yli<0) yli=0;
          if (yli>=it->cmap_y_levels) yli=it->cmap_y_levels-1;
          if (ili<0) ili=0;
          if (ili>=it->cmap_i_levels) ili=it->cmap_i_levels-1;
          if (qli<0) qli=0;
          if (qli>=it->cmap_q_levels) qli=it->cmap_q_levels-1;

          cmi=qli + it->cmap_i_levels*(ili + it->cmap_q_levels*yli);

#ifdef DEBUG
          if ((random()%65536)==0) {
            printf("%0.3f %0.3f %0.3f => %d %d %d => %d\n",
                   interpy, interpi, interpq,
                   yli, ili, qli,
                   cmi);
          }
#endif

          for (j=0; j<it->xrepl; j++) {
            XPutPixel(it->image, x, y,
                      it->colors[cmi]);
            x++;
          }
          if (i >= squishright_i) {
            pixmultinc += pixmultinc/squishdiv;
          }
          i+=pixmultinc;
        }
        while (x<it->usewidth) {
          XPutPixel(it->image, x, y, it->colors[0]);
          x++;
        }
      }
    }
    else {
      struct analogtv_yiq_s *yiq=it->yiq;
      analogtv_ntsc_to_yiq(it, lineno, signal,
                           (scanstart_i>>16)-10, (scanend_i>>16)+10);

      pixbright=it->contrast_control * puramp(it, 1.0, 0.0, 1.0)
        / (0.5+0.5*puheight) * 1024.0/100.0;
      pixmultinc=pixrate;
      i=scanstart_i; rrp=rgb_start;
      while (i<0 && rrp!=rgb_end) {
        rrp[0]=rrp[1]=rrp[2]=0;
        i+=pixmultinc;
        rrp+=3;
      }
      while (i<scanend_i && rrp!=rgb_end) {
        double pixfrac=(i&0xffff)/65536.0;
        double invpixfrac=1.0-pixfrac;
        int pati=i>>16;
        double r,g,b;

        double interpy=(yiq[pati].y*invpixfrac + yiq[pati+1].y*pixfrac);
        double interpi=(yiq[pati].i*invpixfrac + yiq[pati+1].i*pixfrac);
        double interpq=(yiq[pati].q*invpixfrac + yiq[pati+1].q*pixfrac);

        /*
          According to the NTSC spec, Y,I,Q are generated as:

          y=0.30 r + 0.59 g + 0.11 b
          i=0.60 r - 0.28 g - 0.32 b
          q=0.21 r - 0.52 g + 0.31 b

          So if you invert the implied 3x3 matrix you get what standard
          televisions implement with a bunch of resistors (or directly in the
          CRT -- don't ask):

          r = y + 0.948 i + 0.624 q
          g = y - 0.276 i - 0.639 q
          b = y - 1.105 i + 1.729 q
        */

        r=(interpy + 0.948*interpi + 0.624*interpq) * pixbright;
        g=(interpy - 0.276*interpi - 0.639*interpq) * pixbright;
        b=(interpy - 1.105*interpi + 1.729*interpq) * pixbright;
        if (r<0.0) r=0.0;
        if (g<0.0) g=0.0;
        if (b<0.0) b=0.0;
        rrp[0]=r;
        rrp[1]=g;
        rrp[2]=b;

        if (i>=squishright_i) {
          pixmultinc += pixmultinc/squishdiv;
          pixbright += pixbright/squishdiv/2;
        }
        i+=pixmultinc;
        rrp+=3;
      }
      while (rrp != rgb_end) {
        rrp[0]=rrp[1]=rrp[2]=0.0;
        rrp+=3;
      }

      analogtv_blast_imagerow(it, raw_rgb_start, raw_rgb_end,
                              ytop,ybot);
    }
  }
  free(raw_rgb_start);

#if 0
  /* poor attempt at visible retrace */
  for (i=0; i<15; i++) {
    int ytop=(int)((i*it->useheight/15 -
                    it->useheight/2)*puheight) + it->useheight/2;
    int ybot=(int)(((i+1)*it->useheight/15 -
                    it->useheight/2)*puheight) + it->useheight/2;
    int div=it->usewidth*3/2;

    for (x=0; x<it->usewidth; x++) {
      y = ytop + (ybot-ytop)*x / div;
      if (y<0 || y>=it->useheight) continue;
      XPutPixel(it->image, x, y, 0xffffff);
    }
  }
#endif

  if (it->need_clear) {
    XClearWindow(it->dpy, it->window);
    it->need_clear=0;
  }

  if (overall_top>0) {
    XClearArea(it->dpy, it->window,
               it->screen_xo, it->screen_yo,
               it->usewidth, overall_top, 0);
  }
  if (it->useheight > overall_bot) {
    XClearArea(it->dpy, it->window,
               it->screen_xo, it->screen_yo+overall_bot,
               it->usewidth, it->useheight-overall_bot, 0);
  }

  if (overall_bot > overall_top) {
    if (it->use_shm) {
#ifdef HAVE_XSHM_EXTENSION
      XShmPutImage(it->dpy, it->window, it->gc, it->image,
                   0, overall_top,
                   it->screen_xo, it->screen_yo+overall_top,
                   it->usewidth, overall_bot - overall_top,
                   False);
#endif
    } else {
      XPutImage(it->dpy, it->window, it->gc, it->image,
                0, overall_top,
                it->screen_xo, it->screen_yo+overall_top,
                it->usewidth, overall_bot - overall_top);
    }
  }

#ifdef DEBUG
  if (0) {
    struct timeval tv;
    double fps;
    char buf[256];
    gettimeofday(&tv,NULL);

    fps=1.0/((tv.tv_sec - it->last_display_time.tv_sec)
             + 0.000001*(tv.tv_usec - it->last_display_time.tv_usec));
    sprintf(buf, "FPS=%0.1f",fps);
    XDrawString(it->dpy, it->window, it->gc, 50, it->useheight*2/3,
                buf, strlen(buf));

    it->last_display_time=tv;
  }
#endif
}

analogtv_input *
analogtv_input_allocate()
{
  analogtv_input *ret=(analogtv_input *)calloc(1,sizeof(analogtv_input));

  return ret;
}

/*
  This takes a screen image and encodes it as a video camera would,
  including all the bandlimiting and YIQ modulation.
  This isn't especially tuned for speed.
*/

int
analogtv_load_ximage(analogtv *it, analogtv_input *input, XImage *pic_im)
{
  int i,x,y;
  int img_w,img_h;
  int fyx[7],fyy[7];
  int fix[4],fiy[4];
  int fqx[4],fqy[4];
  XColor col1[ANALOGTV_PIC_LEN];
  XColor col2[ANALOGTV_PIC_LEN];
  int multiq[ANALOGTV_PIC_LEN+4];
  int y_overscan=5; /* overscan this much top and bottom */
  int y_scanlength=ANALOGTV_VISLINES+2*y_overscan;

  img_w=pic_im->width;
  img_h=pic_im->height;
  
  for (i=0; i<ANALOGTV_PIC_LEN+4; i++) {
    double phase=90.0-90.0*i;
    double ampl=1.0;
    multiq[i]=(int)(-cos(3.1415926/180.0*(phase-303)) * 4096.0 * ampl);
  }

  for (y=0; y<y_scanlength; y++) {
    int picy1=(y*img_h)/y_scanlength;
    int picy2=(y*img_h + y_scanlength/2)/y_scanlength;

    for (x=0; x<ANALOGTV_PIC_LEN; x++) {
      int picx=(x*img_w)/ANALOGTV_PIC_LEN;
      col1[x].pixel=XGetPixel(pic_im, picx, picy1);
      col2[x].pixel=XGetPixel(pic_im, picx, picy2);
    }
    XQueryColors(it->dpy, it->colormap, col1, ANALOGTV_PIC_LEN);
    XQueryColors(it->dpy, it->colormap, col2, ANALOGTV_PIC_LEN);

    for (i=0; i<7; i++) fyx[i]=fyy[i]=0;
    for (i=0; i<4; i++) fix[i]=fiy[i]=fqx[i]=fqy[i]=0.0;

    for (x=0; x<ANALOGTV_PIC_LEN; x++) {
      int rawy,rawi,rawq;
      int filty,filti,filtq;
      int composite;
      /* Compute YIQ as:
           y=0.30 r + 0.59 g + 0.11 b
           i=0.60 r - 0.28 g - 0.32 b
           q=0.21 r - 0.52 g + 0.31 b
          The coefficients below are in .4 format */

      rawy=( 5*col1[x].red + 11*col1[x].green + 2*col1[x].blue +
             5*col2[x].red + 11*col2[x].green + 2*col2[x].blue)>>7;
      rawi=(10*col1[x].red -  4*col1[x].green - 5*col1[x].blue +
            10*col2[x].red -  4*col2[x].green - 5*col2[x].blue)>>7;
      rawq=( 3*col1[x].red -  8*col1[x].green + 5*col1[x].blue +
             3*col2[x].red -  8*col2[x].green + 5*col2[x].blue)>>7;

      /* Filter y at with a 4-pole low-pass Butterworth filter at 3.5 MHz
         with an extra zero at 3.5 MHz, from
         mkfilter -Bu -Lp -o 4 -a 2.1428571429e-01 0 -Z 2.5e-01 -l */

      fyx[0] = fyx[1]; fyx[1] = fyx[2]; fyx[2] = fyx[3];
      fyx[3] = fyx[4]; fyx[4] = fyx[5]; fyx[5] = fyx[6];
      fyx[6] = (rawy * 1897) >> 16;
      fyy[0] = fyy[1]; fyy[1] = fyy[2]; fyy[2] = fyy[3];
      fyy[3] = fyy[4]; fyy[4] = fyy[5]; fyy[5] = fyy[6];
      fyy[6] = (fyx[0]+fyx[6]) + 4*(fyx[1]+fyx[5]) + 7*(fyx[2]+fyx[4]) + 8*fyx[3]
        + ((-151*fyy[2] + 8115*fyy[3] - 38312*fyy[4] + 36586*fyy[5]) >> 16);
      filty = fyy[6];

      /* Filter I at 1.5 MHz. 3 pole Butterworth from
         mkfilter -Bu -Lp -o 3 -a 1.0714285714e-01 0 */

      fix[0] = fix[1]; fix[1] = fix[2]; fix[2] = fix[3];
      fix[3] = (rawi * 1413) >> 16;
      fiy[0] = fiy[1]; fiy[1] = fiy[2]; fiy[2] = fiy[3];
      fiy[3] = (fix[0]+fix[3]) + 3*(fix[1]+fix[2])
        + ((16559*fiy[0] - 72008*fiy[1] + 109682*fiy[2]) >> 16);
      filti = fiy[3];

      /* Filter Q at 0.5 MHz. 3 pole Butterworth from
         mkfilter -Bu -Lp -o 3 -a 3.5714285714e-02 0 -l */

      fqx[0] = fqx[1]; fqx[1] = fqx[2]; fqx[2] = fqx[3];
      fqx[3] = (rawq * 75) >> 16;
      fqy[0] = fqy[1]; fqy[1] = fqy[2]; fqy[2] = fqy[3];
      fqy[3] = (fqx[0]+fqx[3]) + 3 * (fqx[1]+fqx[2])
        + ((2612*fqy[0] - 9007*fqy[1] + 10453 * fqy[2]) >> 12);
      filtq = fqy[3];


      composite = filty + ((multiq[x] * filti + multiq[x+3] * filtq)>>12);
      composite = ((composite*100)>>14) + ANALOGTV_BLACK_LEVEL;
      if (composite>125) composite=125;
      if (composite<0) composite=0;
      input->signal[y-y_overscan+ANALOGTV_TOP][x+ANALOGTV_PIC_START] = composite;
    }
  }

  return 1;
}

#if 0
void analogtv_channel_noise(analogtv_input *it, analogtv_input *s2)
{
  int x,y,newsig;
  int change=random()%ANALOGTV_V;
  unsigned int fastrnd=random();
  double hso=(int)(random()%1000)-500;
  int yofs=random()%ANALOGTV_V;
  int noise;

  for (y=change; y<ANALOGTV_V; y++) {
    int s2y=(y+yofs)%ANALOGTV_V;
    int filt=0;
    int noiselevel=60000 / (y-change+100);

    it->line_hsync[y] = s2->line_hsync[y] + (int)hso;
    hso *= 0.9;
    for (x=0; x<ANALOGTV_H; x++) {
      FASTRND;
      filt+= (-filt/16) + (int)(fastrnd&0xfff)-0x800;
      noise=(filt*noiselevel)>>16;
      newsig=s2->signal[s2y][x] + noise;
      if (newsig>120) newsig=120;
      if (newsig<0) newsig=0;
      it->signal[y][x]=newsig;
    }
  }
  s2->vsync=yofs;
}
#endif


void analogtv_add_signal(analogtv *it, analogtv_reception *rec)
{
  analogtv_input *inp=rec->input;
  double *ps=it->rx_signal;
  double *pe=it->rx_signal + ANALOGTV_SIGNAL_LEN;
  double *p=ps;
  signed char *ss=&inp->signal[0][0];
  signed char *se=&inp->signal[0][0] + ANALOGTV_SIGNAL_LEN;
  signed char *s=ss + ((unsigned)rec->ofs % ANALOGTV_SIGNAL_LEN);
  int i;
  int ec=it->channel_change_cycles;
  double level=rec->level;
  double hfloss=rec->hfloss;
  unsigned int fastrnd=random();
  double dp[8];

  /* assert((se-ss)%4==0 && (se-s)%4==0); */

  /* duplicate the first line into the Nth line to ease wraparound computation */
  memcpy(inp->signal[ANALOGTV_V], inp->signal[0],
         ANALOGTV_H * sizeof(inp->signal[0][0]));

  for (i=0; i<8; i++) dp[i]=0.0;

  if (ec) {
    double noise_ampl;

    /* Do a big noisy transition. We can make the transition noise of
       high constant strength regardless of signal strength.

       There are two separate state machines. here, One is the noise
       process and the other is the

       We don't bother with the FIR filter here
    */

    noise_ampl = 1.3;

    while (p!=pe && ec>0) {

      double sig0=(double)s[0];
      double noise = ((int)fastrnd-(int)0x7fffffff) * (50.0/(double)0x7fffffff);
      fastrnd = (fastrnd*1103515245+12345) & 0xffffffffu;

      p[0] += sig0 * level * (1.0 - noise_ampl) + noise * noise_ampl;

      noise_ampl *= 0.99995;

      p++;
      s++;
      if (s>=se) s=ss;
      ec--;
    }

  }

  while (p != pe) {
    double sig0,sig1,sig2,sig3,sigr;

    sig0=(double)s[0];
    sig1=(double)s[1];
    sig2=(double)s[2];
    sig3=(double)s[3];

    dp[0]=sig0+sig1+sig2+sig3;

    /* Get the video out signal, and add some ghosting, typical of RF
       monitor cables. This corresponds to a pretty long cable, but
       looks right to me.
    */

    sigr=(dp[1]*rec->ghostfir[0] + dp[2]*rec->ghostfir[1] +
          dp[3]*rec->ghostfir[2] + dp[4]*rec->ghostfir[3]);
    dp[4]=dp[3]; dp[3]=dp[2]; dp[2]=dp[1]; dp[1]=dp[0];

    p[0] += (sig0+sigr + sig2*hfloss) * level;
    p[1] += (sig1+sigr + sig3*hfloss) * level;
    p[2] += (sig2+sigr + sig0*hfloss) * level;
    p[3] += (sig3+sigr + sig1*hfloss) * level;

    p += 4;
    s += 4;
    if (s>=se) s = ss + (s-se);
  }

  it->rx_signal_level =
    sqrt(it->rx_signal_level * it->rx_signal_level +
         (level * level * (1.0 + 4.0*(rec->ghostfir[0] + rec->ghostfir[1] +
                                      rec->ghostfir[2] + rec->ghostfir[3]))));


  it->channel_change_cycles=0;

}

#ifdef FIXME
/* add hash */
  if (it->hashnoise_times[lineno]) {
    int hnt=it->hashnoise_times[lineno] - input->line_hsync[lineno];

    if (hnt>=0 && hnt<ANALOGTV_PIC_LEN) {
      double maxampl=1.0;
      double cur=frand(150.0)-20.0;
      int len=random()%15+3;
      if (len > ANALOGTV_PIC_LEN-hnt) len=ANALOGTV_PIC_LEN-hnt;
      for (i=0; i<len; i++) {
        double sig=signal[hnt];

        sig += cur*maxampl;
        cur += frand(5.0)-5.0;
        maxampl = maxampl*0.9;

        signal[hnt]=sig;
        hnt++;
      }
    }
  }
#endif


void analogtv_init_signal(analogtv *it, double noiselevel)
{
  double *ps=it->rx_signal;
  double *pe=it->rx_signal + ANALOGTV_SIGNAL_LEN;
  double *p=ps;
  unsigned int fastrnd=random();
  double nm1=0.0,nm2=0.0;
  double noisemul = sqrt(noiselevel*150)/(double)0x7fffffff;

  while (p != pe) {
    nm2=nm1;
    nm1 = ((int)fastrnd-(int)0x7fffffff) * noisemul;
    *p++ = nm1*nm2;
    fastrnd = (fastrnd*1103515245+12345) & 0xffffffffu;
  }

  it->rx_signal_level = noiselevel;
}

void
analogtv_reception_update(analogtv_reception *rec)
{
  int i;

  if (rec->multipath > 0.0) {
    for (i=0; i<ANALOGTV_GHOSTFIR_LEN; i++) {
      rec->ghostfir2[i] +=
        -(rec->ghostfir2[i]/16.0) + rec->multipath * (frand(0.02)-0.01);
    }
    if (random()%20==0) {
      rec->ghostfir2[random()%(ANALOGTV_GHOSTFIR_LEN)]
        = rec->multipath * (frand(0.08)-0.04);
    }
    for (i=0; i<ANALOGTV_GHOSTFIR_LEN; i++) {
      rec->ghostfir[i] = 0.8*rec->ghostfir[i] + 0.2*rec->ghostfir2[i];
    }

    if (0) {
      rec->hfloss2 += -(rec->hfloss2/16.0) + rec->multipath * (frand(0.08)-0.04);
      rec->hfloss = 0.5*rec->hfloss + 0.5*rec->hfloss2;
    }

  } else {
    for (i=0; i<ANALOGTV_GHOSTFIR_LEN; i++) {
      rec->ghostfir[i] = (i>=ANALOGTV_GHOSTFIR_LEN/2) ? ((i&1) ? +0.04 : -0.08) /ANALOGTV_GHOSTFIR_LEN
        : 0.0;
    }
  }
}


/* jwz: since MacOS doesn't have "6x10", I dumped this font to an XBM...
 */

#include "images/6x10font.xbm"

void
analogtv_make_font(Display *dpy, Window window, analogtv_font *f,
                   int w, int h, char *fontname)
{
  int i;
  XFontStruct *font;
  Pixmap text_pm;
  GC gc;
  XGCValues gcv;
  XWindowAttributes xgwa;

  f->char_w = w;
  f->char_h = h;

  XGetWindowAttributes (dpy, window, &xgwa);

  if (fontname && !strcmp (fontname, "6x10")) {

    text_pm = XCreatePixmapFromBitmapData (dpy, window,
                                           (char *) font6x10_bits,
                                           font6x10_width,
                                           font6x10_height,
                                           1, 0, 1);
    f->text_im = XGetImage(dpy, text_pm, 0, 0, font6x10_width, font6x10_height,
                           1, XYPixmap);
    XFreePixmap(dpy, text_pm);

  } else if (fontname) {

    font = XLoadQueryFont (dpy, fontname);
    if (!font) {
      fprintf(stderr, "analogtv: can't load font %s\n", fontname);
      abort();
    }

    text_pm=XCreatePixmap(dpy, window, 256*f->char_w, f->char_h, 1);

    memset(&gcv, 0, sizeof(gcv));
    gcv.foreground=1;
    gcv.background=0;
    gcv.font=font->fid;
    gc=XCreateGC(dpy, text_pm, GCFont|GCBackground|GCForeground, &gcv);

    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, text_pm, gc, 0, 0, 256*f->char_w, f->char_h);
    XSetForeground(dpy, gc, 1);
    for (i=0; i<256; i++) {
      char c=i;
      int x=f->char_w*i+1;
      int y=f->char_h*8/10;
      XDrawString(dpy, text_pm, gc, x, y, &c, 1);
    }
    f->text_im = XGetImage(dpy, text_pm, 0, 0, 256*f->char_w, f->char_h,
                           1, XYPixmap);
# if 0
    XWriteBitmapFile(dpy, "/tmp/tvfont.xbm", text_pm, 
                     256*f->char_w, f->char_h, -1, -1);
# endif
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, text_pm);
  } else {
    f->text_im = XCreateImage(dpy, xgwa.visual, 1, XYPixmap, 0, 0,
                              256*f->char_w, f->char_h, 8, 0);
    f->text_im->data = (char *)calloc(f->text_im->height,
                                      f->text_im->bytes_per_line);

  }
  f->x_mult=4;
  f->y_mult=2;
}

int
analogtv_font_pixel(analogtv_font *f, int c, int x, int y)
{
  if (x<0 || x>=f->char_w) return 0;
  if (y<0 || y>=f->char_h) return 0;
  if (c<0 || c>=256) return 0;
  return XGetPixel(f->text_im, c*f->char_w + x, y) ? 1 : 0;
}

void
analogtv_font_set_pixel(analogtv_font *f, int c, int x, int y, int value)
{
  if (x<0 || x>=f->char_w) return;
  if (y<0 || y>=f->char_h) return;
  if (c<0 || c>=256) return;

  XPutPixel(f->text_im, c*f->char_w + x, y, value);
}

void
analogtv_font_set_char(analogtv_font *f, int c, char *s)
{
  int value,x,y;

  if (c<0 || c>=256) return;

  for (y=0; y<f->char_h; y++) {
    for (x=0; x<f->char_w; x++) {
      if (!*s) return;
      value=(*s==' ') ? 0 : 1;
      analogtv_font_set_pixel(f, c, x, y, value);
      s++;
    }
  }
}

void
analogtv_lcp_to_ntsc(double luma, double chroma, double phase, int ntsc[4])
{
  int i;
  for (i=0; i<4; i++) {
    double w=90.0*i + phase;
    double val=luma + chroma * (cos(3.1415926/180.0*w));
    if (val<0.0) val=0.0;
    if (val>127.0) val=127.0;
    ntsc[i]=(int)val;
  }
}

void
analogtv_draw_solid(analogtv_input *input,
                    int left, int right, int top, int bot,
                    int ntsc[4])
{
  int x,y;

  if (right-left<4) right=left+4;
  if (bot-top<1) bot=top+1;

  for (y=top; y<bot; y++) {
    for (x=left; x<right; x++) {
      input->signal[y][x] = ntsc[x&3];
    }
  }
}


void
analogtv_draw_solid_rel_lcp(analogtv_input *input,
                            double left, double right, double top, double bot,
                            double luma, double chroma, double phase)
{
  int ntsc[4];

  int topi=(int)(ANALOGTV_TOP + ANALOGTV_VISLINES*top);
  int boti=(int)(ANALOGTV_TOP + ANALOGTV_VISLINES*bot);
  int lefti=(int)(ANALOGTV_VIS_START + ANALOGTV_VIS_LEN*left);
  int righti=(int)(ANALOGTV_VIS_START + ANALOGTV_VIS_LEN*right);

  analogtv_lcp_to_ntsc(luma, chroma, phase, ntsc);
  analogtv_draw_solid(input, lefti, righti, topi, boti, ntsc);
}


void
analogtv_draw_char(analogtv_input *input, analogtv_font *f,
                   int c, int x, int y, int ntsc[4])
{
  int yc,xc,ys,xs,pix;

  for (yc=0; yc<f->char_h; yc++) {
    for (ys=y + yc*f->y_mult; ys<y + (yc+1)*f->y_mult; ys++) {
      if (ys<0 || ys>=ANALOGTV_V) continue;

      for (xc=0; xc<f->char_w; xc++) {
        pix=analogtv_font_pixel(f, c, xc, yc);

        for (xs=x + xc*f->x_mult; xs<x + (xc+1)*f->x_mult; xs++) {
          if (xs<0 || xs>=ANALOGTV_H) continue;
          if (pix) {
            input->signal[ys][xs] = ntsc[xs&3];
          }
        }
      }
    }
  }
}

void
analogtv_draw_string(analogtv_input *input, analogtv_font *f,
                     char *s, int x, int y, int ntsc[4])
{
  while (*s) {
    analogtv_draw_char(input, f, *s, x, y, ntsc);
    x += f->char_w * 4;
    s++;
  }
}

void
analogtv_draw_string_centered(analogtv_input *input, analogtv_font *f,
                              char *s, int x, int y, int ntsc[4])
{
  int width=strlen(s) * f->char_w * 4;
  x -= width/2;

  analogtv_draw_string(input, f, s, x, y, ntsc);
}


static const char hextonib[128] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
                                   0, 10,11,12,13,14,15,0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 10,11,12,13,14,15,0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/*
  Much of this function was adapted from logo.c
 */
void
analogtv_draw_xpm(analogtv *tv, analogtv_input *input,
                  const char * const *xpm, int left, int top)
{
  int xpmw,xpmh;
  int x,y,tvx,tvy,i;
  int rawy,rawi,rawq;
  int ncolors, nbytes;
  char dummyc;
  struct {
    int r; int g; int b;
  } cmap[256];


  if (4 != sscanf ((const char *) *xpm,
                   "%d %d %d %d %c",
                   &xpmw, &xpmh, &ncolors, &nbytes, &dummyc))
    abort();
  if (ncolors < 1 || ncolors > 255)
    abort();
  if (nbytes != 1) /* a serious limitation */
    abort();
  xpm++;

  for (i = 0; i < ncolors; i++) {
    const char *line = *xpm;
    int colori = ((unsigned char)*line++)&0xff;
    while (*line)
      {
        int r, g, b;
        char which;
        while (*line == ' ' || *line == '\t')
          line++;
        which = *line++;
        if (which != 'c' && which != 'm')
          abort();
        while (*line == ' ' || *line == '\t')
          line++;
        if (!strncasecmp(line, "None", 4))
          {
            r = g = b = -1;
            line += 4;
          }
        else
          {
            if (*line == '#')
              line++;
            r = (hextonib[(int) line[0]] << 4) | hextonib[(int) line[1]];
            line += 2;
            g = (hextonib[(int) line[0]] << 4) | hextonib[(int) line[1]];
            line += 2;
            b = (hextonib[(int) line[0]] << 4) | hextonib[(int) line[1]];
            line += 2;
          }

        if (which == 'c')
          {
            cmap[colori].r = r;
            cmap[colori].g = g;
            cmap[colori].b = b;
          }
      }

    xpm++;
  }

  for (y=0; y<xpmh; y++) {
    const char *line = *xpm++;
    tvy=y+top;
    if (tvy<ANALOGTV_TOP || tvy>=ANALOGTV_BOT) continue;

    for (x=0; x<xpmw; x++) {
      int cbyte=((unsigned char)line[x])&0xff;
      int ntsc[4];
      tvx=x*4+left;
      if (tvx<ANALOGTV_PIC_START || tvx+4>ANALOGTV_PIC_END) continue;

      rawy=( 5*cmap[cbyte].r + 11*cmap[cbyte].g + 2*cmap[cbyte].b) / 64;
      rawi=(10*cmap[cbyte].r -  4*cmap[cbyte].g - 5*cmap[cbyte].b) / 64;
      rawq=( 3*cmap[cbyte].r -  8*cmap[cbyte].g + 5*cmap[cbyte].b) / 64;

      ntsc[0]=rawy+rawq;
      ntsc[1]=rawy-rawi;
      ntsc[2]=rawy-rawq;
      ntsc[3]=rawy+rawi;

      for (i=0; i<4; i++) {
        if (ntsc[i]>ANALOGTV_WHITE_LEVEL) ntsc[i]=ANALOGTV_WHITE_LEVEL;
        if (ntsc[i]<ANALOGTV_BLACK_LEVEL) ntsc[i]=ANALOGTV_BLACK_LEVEL;
      }

      input->signal[tvy][tvx+0]= ntsc[(tvx+0)&3];
      input->signal[tvy][tvx+1]= ntsc[(tvx+1)&3];
      input->signal[tvy][tvx+2]= ntsc[(tvx+2)&3];
      input->signal[tvy][tvx+3]= ntsc[(tvx+3)&3];
    }
  }
}
