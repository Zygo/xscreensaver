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

#ifndef _XSCREENSAVER_ANALOGTV_H
#define _XSCREENSAVER_ANALOGTV_H

#include "xshm.h"

/*
  You'll need these to generate standard NTSC TV signals
 */
enum {
  /* We don't handle interlace here */
  ANALOGTV_V=262,
  ANALOGTV_TOP=30,
  ANALOGTV_VISLINES=200,
  ANALOGTV_BOT=ANALOGTV_TOP + ANALOGTV_VISLINES,

  /* This really defines our sampling rate, 4x the colorburst
     frequency. Handily equal to the Apple II's dot clock.
     You could also make a case for using 3x the colorburst freq,
     but 4x isn't hard to deal with. */
  ANALOGTV_H=912,

  /* Each line is 63500 nS long. The sync pulse is 4700 nS long, etc.
     Define sync, back porch, colorburst, picture, and front porch
     positions */
  ANALOGTV_SYNC_START=0,
  ANALOGTV_BP_START=4700*ANALOGTV_H/63500,
  ANALOGTV_CB_START=5800*ANALOGTV_H/63500,
  /* signal[row][ANALOGTV_PIC_START] is the first displayed pixel */
  ANALOGTV_PIC_START=9400*ANALOGTV_H/63500,
  ANALOGTV_PIC_LEN=52600*ANALOGTV_H/63500,
  ANALOGTV_FP_START=62000*ANALOGTV_H/63500,
  ANALOGTV_PIC_END=ANALOGTV_FP_START,

  /* TVs scan past the edges of the picture tube, so normally you only
     want to use about the middle 3/4 of the nominal scan line.
  */
  ANALOGTV_VIS_START=ANALOGTV_PIC_START + (ANALOGTV_PIC_LEN*1/8),
  ANALOGTV_VIS_END=ANALOGTV_PIC_START + (ANALOGTV_PIC_LEN*7/8),
  ANALOGTV_VIS_LEN=ANALOGTV_VIS_END-ANALOGTV_VIS_START,

  ANALOGTV_HASHNOISE_LEN=6,

  ANALOGTV_GHOSTFIR_LEN=4,

  /* analogtv.signal is in IRE units, as defined below: */
  ANALOGTV_WHITE_LEVEL=100,
  ANALOGTV_GRAY50_LEVEL=55,
  ANALOGTV_GRAY30_LEVEL=35,
  ANALOGTV_BLACK_LEVEL=10,
  ANALOGTV_BLANK_LEVEL=0,
  ANALOGTV_SYNC_LEVEL=-40,
  ANALOGTV_CB_LEVEL=20,

  ANALOGTV_SIGNAL_LEN=ANALOGTV_V*ANALOGTV_H,

  /* The number of intensity levels we deal with for gamma correction &c */
  ANALOGTV_CV_MAX=1024
};

typedef struct analogtv_input_s {
  char signal[ANALOGTV_V+1][ANALOGTV_H];

  int do_teletext;

  /* for client use */
  void (*updater)(struct analogtv_input_s *inp);
  void *client_data;
  double next_update_time;

} analogtv_input;

typedef struct analogtv_font_s {
  XImage *text_im;
  int char_w, char_h;
  int x_mult, y_mult;
} analogtv_font;

typedef struct analogtv_reception_s {

  analogtv_input *input;
  double ofs;
  double level;
  double multipath;
  double freqerr;

  double ghostfir[ANALOGTV_GHOSTFIR_LEN];
  double ghostfir2[ANALOGTV_GHOSTFIR_LEN];

  double hfloss;
  double hfloss2;

} analogtv_reception;

/*
  The rest of this should be considered mostly opaque to the analogtv module.
 */

typedef struct analogtv_s {

  Display *dpy;
  Window window;
  Screen *screen;
  XWindowAttributes xgwa;

#if 0
  unsigned int onscreen_signature[ANALOGTV_V];
#endif

  int n_colors;

  int interlace;
  int interlace_counter;

  double agclevel;

  /* If you change these, call analogtv_set_demod */
  double tint_control,color_control,brightness_control,contrast_control;
  double height_control, width_control, squish_control;
  double horiz_desync;
  double squeezebottom;
  double powerup;

  /* internal cache */
  int blur_mult;

  /* For fast display, set fakeit_top, fakeit_bot to
     the scanlines (0..ANALOGTV_V) that can be preserved on screen.
     fakeit_scroll is the number of scan lines to scroll it up,
     or 0 to not scroll at all. It will DTRT if asked to scroll from
     an offscreen region.
  */
  int fakeit_top;
  int fakeit_bot;
  int fakeit_scroll;
  int redraw_all;

  int use_shm,use_cmap,use_color;
  int bilevel_signal;

#ifdef HAVE_XSHM_EXTENSION
  XShmSegmentInfo shm_info;
#endif
  int visdepth,visclass,visbits;
  int red_invprec,red_shift,red_mask;
  int green_invprec,green_shift,green_mask;
  int blue_invprec,blue_shift,blue_mask;

  Colormap colormap;
  int usewidth,useheight,xrepl,subwidth;
  XImage *image; /* usewidth * useheight */
  GC gc;
  int screen_xo,screen_yo; /* centers image in window */

  void (*event_handler)(Display *dpy, XEvent *event);
  int (*key_handler)(Display *dpy, XEvent *event,void *key_data);
  void *key_data;

  int flutter_horiz_desync;
  int flutter_tint;

  struct timeval last_display_time;
  int need_clear;


  /* Add hash (in the radio sense, not the programming sense.) These
     are the small white streaks that appear in quasi-regular patterns
     all over the screen when someone is running the vacuum cleaner or
     the blender. We also set shrinkpulse for one period which
     squishes the image horizontally to simulate the temporary line
     voltate drop when someone turns on a big motor */
  double hashnoise_rpm;
  int hashnoise_counter;
  int hashnoise_times[ANALOGTV_V];
  int hashnoise_signal[ANALOGTV_V];
  int hashnoise_on;
  int hashnoise_enable;
  int shrinkpulse;

  double crtload[ANALOGTV_V];

  unsigned int red_values[ANALOGTV_CV_MAX];
  unsigned int green_values[ANALOGTV_CV_MAX];
  unsigned int blue_values[ANALOGTV_CV_MAX];

  struct analogtv_yiq_s {
    float y,i,q;
  } yiq[ANALOGTV_PIC_LEN+10];

  unsigned long colors[256];
  int cmap_y_levels;
  int cmap_i_levels;
  int cmap_q_levels;

  int cur_hsync;
  int line_hsync[ANALOGTV_V];
  int cur_vsync;
  double cb_phase[4];
  double line_cb_phase[ANALOGTV_V][4];

  int channel_change_cycles;
  double rx_signal_level;
  double rx_signal[ANALOGTV_SIGNAL_LEN + 2*ANALOGTV_H];

} analogtv;


analogtv *analogtv_allocate(Display *dpy, Window window);
analogtv_input *analogtv_input_allocate(void);

/* call if window size changes */
void analogtv_reconfigure(analogtv *it);

void analogtv_set_defaults(analogtv *it, char *prefix);
void analogtv_release(analogtv *it);
int analogtv_set_demod(analogtv *it);
void analogtv_setup_frame(analogtv *it);
void analogtv_setup_sync(analogtv_input *input, int do_cb, int do_ssavi);
void analogtv_draw(analogtv *it);

int analogtv_load_ximage(analogtv *it, analogtv_input *input, XImage *pic_im);

void analogtv_reception_update(analogtv_reception *inp);

void analogtv_init_signal(analogtv *it, double noiselevel);
void analogtv_add_signal(analogtv *it, analogtv_reception *rec);

void analogtv_setup_teletext(analogtv_input *input);


/* Functions for rendering content into an analogtv_input */

void analogtv_make_font(Display *dpy, Window window,
                        analogtv_font *f, int w, int h, char *fontname);
int analogtv_font_pixel(analogtv_font *f, int c, int x, int y);
void analogtv_font_set_pixel(analogtv_font *f, int c, int x, int y, int value);
void analogtv_font_set_char(analogtv_font *f, int c, char *s);
void analogtv_lcp_to_ntsc(double luma, double chroma, double phase,
                          int ntsc[4]);


void analogtv_draw_solid(analogtv_input *input,
                         int left, int right, int top, int bot,
                         int ntsc[4]);

void analogtv_draw_solid_rel_lcp(analogtv_input *input,
                                 double left, double right,
                                 double top, double bot,
                                 double luma, double chroma, double phase);

void analogtv_draw_char(analogtv_input *input, analogtv_font *f,
                        int c, int x, int y, int ntsc[4]);
void analogtv_draw_string(analogtv_input *input, analogtv_font *f,
                          char *s, int x, int y, int ntsc[4]);
void analogtv_draw_string_centered(analogtv_input *input, analogtv_font *f,
                                   char *s, int x, int y, int ntsc[4]);
void analogtv_draw_xpm(analogtv *tv, analogtv_input *input,
                       const char * const *xpm, int left, int top);

int analogtv_handle_events (analogtv *it);

#ifdef HAVE_XSHM_EXTENSION
#define ANALOGTV_DEFAULTS_SHM "*useSHM:           True",
#else
#define ANALOGTV_DEFAULTS_SHM
#endif

#define ANALOGTV_DEFAULTS \
  "*TVColor:         70", \
  "*TVTint:          5",  \
  "*TVBrightness:    2",  \
  "*TVContrast:      150",\
  "*Background:      Black", \
  "*use_cmap:        0",  \
  "*geometry:	     800x600", \
  ANALOGTV_DEFAULTS_SHM

#define ANALOGTV_OPTIONS \
  { "-use-cmap",        ".use_cmap",  XrmoptionSepArg, 0 },


#endif /* _XSCREENSAVER_ANALOGTV_H */
