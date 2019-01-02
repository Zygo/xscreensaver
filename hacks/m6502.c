/* -*- indent-tabs-mode:nil -*-
 * Copyright (c) 2007 Jeremy English <jhe@jeremyenglish.org>
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 * 
 * Created: 07-May-2007 
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if defined(HAVE_STDINT_H)
#include <stdint.h> 
#elif defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif
#include <string.h>
#include "screenhack.h"
#include "analogtv.h"
#include "asm6502.h"

# ifdef __GNUC__
  __extension__  /* don't warn about "string length is greater than the length
                    ISO C89 compilers are required to support" when includng
                    the following data file... */
# endif
static const char * const demo_files[] = {
# include "m6502.h"
};


#ifndef HAVE_MOBILE
# define READ_FILES
#endif


/* We want to paint on a 32 by 32 grid of pixels. We will needed to
   divided the screen up into chuncks */
enum {
  SCREEN_W = ANALOGTV_VIS_LEN,
  SCREEN_H = ANALOGTV_VISLINES,
  NUM_PROGS = 9
};

struct state {
  Display *dpy;
  Window window;
  
  Bit8 pixels[32][32];

  machine_6502 *machine;

  analogtv *tv;
  analogtv_input *inp;
  analogtv_reception reception;
  int pixw; /* pixel width */
  int pixh;/* pixel height */
  int topb;/* top boarder */
  int field_ntsc[4];/* used for clearing the screen*/ 
  double dt;/* how long to wait before changing the demo*/
  int which;/* the program to run*/
  int demos;/* number of demos included */
  double start_time;
  int reset_p;
  char *file;
  double last_frame, last_delay;
  unsigned ips;
};

static void
plot6502(Bit8 x, Bit8 y, Bit8 color, void *closure)
{
  struct state *st = (struct state *) closure;
  st->pixels[x][y] = color;
}

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


static void 
start_rand_bin_prog(machine_6502 *machine, struct state *st){
  int n = st->which;
  while(n == st->which)
    n = random() % st->demos;
  st->which = n;
  m6502_start_eval_string(machine, demo_files[st->which], plot6502, st);
}


/*
 * double_time ()
 *
 * returns the current time as a floating-point value
 */
static double double_time(void) {
  struct timeval t;
  double f;
#if GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&t, NULL);
#else
  gettimeofday(&t);
#endif
  f = ((double)t.tv_sec) + t.tv_usec*1e-6;
  return f;
}

static void *
m6502_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int n = get_float_resource(dpy, "displaytime", "Displaytime");
  int dh;
  st->demos = countof(demo_files);
  st->which = random() % st->demos;
  st->dt = n;
  st->dpy = dpy;
  st->window = window;
  st->tv=analogtv_allocate(st->dpy, st->window);
  analogtv_set_defaults(st->tv, "");
  
  st->machine = m6502_build();
  st->inp=analogtv_input_allocate();
  analogtv_setup_sync(st->inp, 1, 0);
  
  st->reception.input = st->inp;
  st->reception.level = 2.0;
  st->reception.ofs=0;
  
  st->reception.multipath=0.0;
  st->pixw = SCREEN_W / 32;
  st->pixh = SCREEN_H / 32;
  dh = SCREEN_H % 32;
  st->topb = dh / 2;

  st->last_frame = double_time();
  st->last_delay = 0;
  st->ips = get_integer_resource(dpy, "ips", "IPS");

#ifdef READ_FILES
  st->file = get_string_resource (dpy, "file", "File");
#endif

  st->reset_p = 1;

  analogtv_lcp_to_ntsc(ANALOGTV_BLACK_LEVEL, 0.0, 0.0, st->field_ntsc);

  analogtv_draw_solid(st->inp,
                      ANALOGTV_VIS_START, ANALOGTV_VIS_END,
                      ANALOGTV_TOP, ANALOGTV_BOT,
                      st->field_ntsc);

  return st;
}

static void
paint_pixel(struct state *st, int x, int y, int idx)
{
  double clr_tbl[16][3] = {
    {  0,   0,   0},
    {255, 255, 255},
    {136,   0,   0},
    {170, 255, 238},
    {204,  68, 204},
    {  0, 204,  85},
    {  0,   0, 170},
    {238, 238, 119},
    {221, 136,  85},
    {102,  68,   0},
    {255, 119, 119},
    { 51,  51,  51},
    {119, 119, 119},
    {170, 255, 102},
    {  0, 136, 255},
    {187, 187, 187}
  };
  int ntsc[4], i;
  int rawy,rawi,rawq;
  /* RGB conversion taken from analogtv draw xpm */
  rawy=( 5*clr_tbl[idx][0] + 11*clr_tbl[idx][1] + 2*clr_tbl[idx][2]) / 64;
  rawi=(10*clr_tbl[idx][0] -  4*clr_tbl[idx][1] - 5*clr_tbl[idx][2]) / 64;
  rawq=( 3*clr_tbl[idx][0] -  8*clr_tbl[idx][1] + 5*clr_tbl[idx][2]) / 64;

  ntsc[0]=rawy+rawq;
  ntsc[1]=rawy-rawi;
  ntsc[2]=rawy-rawq;
  ntsc[3]=rawy+rawi;

  for (i=0; i<4; i++) {
    if (ntsc[i]>ANALOGTV_WHITE_LEVEL) ntsc[i]=ANALOGTV_WHITE_LEVEL;
    if (ntsc[i]<ANALOGTV_BLACK_LEVEL) ntsc[i]=ANALOGTV_BLACK_LEVEL;
  }

      
  x *= st->pixw;
  y *= st->pixh;
  y += st->topb;
  analogtv_draw_solid(st->inp,
		      ANALOGTV_VIS_START + x, ANALOGTV_VIS_START + x + st->pixw,
		      ANALOGTV_TOP + y, ANALOGTV_TOP + y + st->pixh, ntsc);			      
}

static unsigned long
m6502_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  unsigned int x = 0, y = 0;
  double now, last_delay = st->last_delay >= 0 ? st->last_delay : 0;
  double insno = st->ips * ((1 / 29.97) + last_delay - st->last_delay);
  const analogtv_reception *reception = &st->reception;

  if (st->reset_p){ /* do something more interesting here XXX */
    st->reset_p = 0;
    for(x = 0; x < 32; x++)
      for(y = 0; y < 32; y++)
        st->pixels[x][y] = 0;
    st->start_time = st->last_frame + last_delay;

#ifdef READ_FILES
    if (st->file && *st->file)
      m6502_start_eval_file(st->machine, st->file, plot6502, st);
    else
#endif
      start_rand_bin_prog(st->machine,st);
  }

  if (insno < 10)
    insno = 10;
  else if (insno > 100000) /* Real 6502 went no faster than 3 MHz. */
    insno = 100000;
  m6502_next_eval(st->machine,insno);

  for (x = 0; x < 32; x++)
    for (y = 0; y < 32; y++)
      paint_pixel(st,x,y,st->pixels[x][y]);
  
  analogtv_reception_update(&st->reception);
  analogtv_draw(st->tv, 0.04, &reception, 1);
  now = double_time();
  st->last_delay = (1 / 29.97) + st->last_frame + last_delay - now;
  st->last_frame = now;

  if (now - st->start_time > st->dt)
    st->reset_p = 1;

  return st->last_delay >= 0 ? st->last_delay * 1e6 : 0;
}




static const char *m6502_defaults [] = {
  ".background:      black",
  ".foreground:      white",
  "*file:",
  "*displaytime:     30.0",     /* demoscene: 24s, dmsc: 48s, sierpinsky: 26s
                                   sflake, two runs: 35s
                                 */
  "*ips:             15000",    /* Actual MOS 6502 ran at least 1 MHz. */
  ANALOGTV_DEFAULTS
  0
};

static XrmOptionDescRec m6502_options [] = {
  { "-file",           ".file",     XrmoptionSepArg, 0 },
  { "-displaytime",    ".displaytime", XrmoptionSepArg, 0},
  { "-ips",            ".ips",         XrmoptionSepArg, 0},
  ANALOGTV_OPTIONS
  { 0, 0, 0, 0 }
};

static void
m6502_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  analogtv_reconfigure (st->tv);
}

static Bool
m6502_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->reset_p = 1;
      return True;
    }
  return False;
}

static void
m6502_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  m6502_destroy6502(st->machine);
  analogtv_release(st->tv);
  free (st->inp);
  free (st->file);
  free (st);
}

XSCREENSAVER_MODULE ("m6502", m6502)
