/* xanalogtv, Copyright (c) 2003 Trevor Blackwell <tlb@tlb.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 *
 * Simulate test patterns on an analog TV. Concept similar to xteevee
 * in this distribution, but a totally different implementation based
 * on the simulation of an analog TV set in utils/analogtv.c. Much
 * more realistic, but needs more video card bandwidth.
 *
 * It flips around through simulated channels 2 through 13. Some show
 * pictures from your images directory, some show color bars, and some
 * just have static. Some channels receive two stations simultaneously
 * so you see a ghostly, misaligned image.
 *
 * It's easy to add some test patterns by compiling in an XPM, but I
 * can't find any that are clearly freely redistributable.
 *
 */

#include <math.h>
#include "screenhack.h"
#include "xpm-pixmap.h"
#include "analogtv.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Intrinsic.h>

#include "images/logo-50.xpm"

/* #define DEBUG 1 */
/* #define USE_TEST_PATTERNS */

#define countof(x) (sizeof((x))/sizeof((*x)))

static analogtv *tv=NULL;

analogtv_font ugly_font;

static void
update_smpte_colorbars(analogtv_input *input)
{
  int col;
  int xpos, ypos;
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
  analogtv_setup_teletext(input);

  for (col=0; col<7; col++) {
    analogtv_draw_solid_rel_lcp(input, col*(1.0/7.0), (col+1)*(1.0/7.0), 0.00, 0.68, 
                                top_cb_table[col][0], 
                                top_cb_table[col][1], top_cb_table[col][2]);
    
    analogtv_draw_solid_rel_lcp(input, col*(1.0/7.0), (col+1)*(1.0/7.0), 0.68, 0.75, 
                                mid_cb_table[col][0], 
                                mid_cb_table[col][1], mid_cb_table[col][2]);
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


  ypos=ANALOGTV_V/5;
  xpos=ANALOGTV_VIS_START + ANALOGTV_VIS_LEN/2;

  {
    char localname[256];
    if (gethostname (localname, sizeof (localname))==0) {
      localname[sizeof(localname)-1]=0; /* "The returned name is null-
                                           terminated unless insufficient 
                                           space is provided" */
      localname[24]=0; /* limit length */

      analogtv_draw_string_centered(input, &ugly_font, localname,
                                    xpos, ypos, black_ntsc);
    }
  }
  ypos += ugly_font.char_h*5/2;

  analogtv_draw_xpm(tv, input,
                    logo_50_xpm, xpos - 100, ypos);

  ypos += 58;

#if 0
  analogtv_draw_string_centered(input, &ugly_font, "Please Stand By", xpos, ypos);
  ypos += ugly_font.char_h*4;
#endif

  {
    char timestamp[256];
    time_t t = time ((time_t *) 0);
    struct tm *tm = localtime (&t);

    /* Y2K: It is OK for this to use a 2-digit year because it's simulating a
       TV display and is purely decorative. */
    strftime(timestamp, sizeof(timestamp)-1, "%y.%m.%d %H:%M:%S ", tm);
    analogtv_draw_string_centered(input, &ugly_font, timestamp,
                                  xpos, ypos, black_ntsc);
  }

  
  input->next_update_time += 1.0;
}

#if 0
static void
draw_color_square(analogtv_input *input)
{
  double xs,ys;

  analogtv_draw_solid_rel_lcp(input, 0.0, 1.0, 0.0, 1.0,
                              30.0, 0.0, 0.0);
  
  for (xs=0.0; xs<0.9999; xs+=1.0/15.0) {
    analogtv_draw_solid_rel_lcp(input, xs, xs, 0.0, 1.0,
                                100.0, 0.0, 0.0);
  }

  for (ys=0.0; ys<0.9999; ys+=1.0/11.0) {
    analogtv_draw_solid_rel_lcp(input, 0.0, 1.0, ys, ys,
                                100.0, 0.0, 0.0);
  }

  for (ys=0.0; ys<0.9999; ys+=0.01) {
    
    analogtv_draw_solid_rel_lcp(input, 0.0/15, 1.0/15, ys, ys+0.01,
                                40.0, 45.0, 103.5*(1.0-ys) + 347.0*ys);

    analogtv_draw_solid_rel_lcp(input, 14.0/15, 15.0/15, ys, ys+0.01,
                                40.0, 45.0, 103.5*(ys) + 347.0*(1.0-ys));
  }

  for (ys=0.0; ys<0.9999; ys+=0.02) {
    analogtv_draw_solid_rel_lcp(input, 1.0/15, 2.0/15, ys*2.0/11.0+1.0/11.0, 
                                (ys+0.01)*2.0/11.0+1.0/11.0,
                                100.0*(1.0-ys), 0.0, 0.0);
  }


}
#endif

char *progclass = "XAnalogTV";

char *defaults [] = {
  ".background:	     black",
  ".foreground:	     white",
  "*delay:	     5",
  ANALOGTV_DEFAULTS
  0,
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  ANALOGTV_OPTIONS
  { 0, 0, 0, 0 }
};


#ifdef USE_TEST_PATTERNS

#include "images/earth.xpm"

char **test_patterns[] = {
  earth_xpm,
};

#endif


enum {
  N_CHANNELS=12, /* Channels 2 through 13 on VHF */
  MAX_MULTICHAN=2
}; 

typedef struct chansetting_s {

  analogtv_reception recs[MAX_MULTICHAN];
  double noise_level;

  int dur;
} chansetting;

static struct timeval basetime;

static int
getticks(void)
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return ((tv.tv_sec - basetime.tv_sec)*1000 +
          (tv.tv_usec - basetime.tv_usec)/1000);
}


/* The first time we grab an image, do it the default way.
   The second and subsequent times, add "-no-desktop" to the command.
   That way we don't have to watch the window un-map 5+ times in a row.
   Also, we end up with the desktop on only one channel, and pictures
   on all the others (or colorbars, if no imageDirectory is set.)
 */
static void
hack_resources (void)
{
  static int count = -1;
  count++;

  if (count == 0)
    return;
  else if (count == 1)
    {
      char *res = "desktopGrabber";
      char *val = get_string_resource (res, "DesktopGrabber");
      char buf1[255];
      char buf2[255];
      XrmValue value;
      sprintf (buf1, "%.100s.%.100s", progclass, res);
      sprintf (buf2, "%.200s -no-desktop", val);
      value.addr = buf2;
      value.size = strlen(buf2);
      XrmPutResource (&db, buf1, "String", &value);
    }
}


int
analogtv_load_random_image(analogtv *it, analogtv_input *input)
{
  Pixmap pixmap;
  XImage *image=NULL;
  int width=ANALOGTV_PIC_LEN;
  int height=width*3/4;
  int rc;

  pixmap=XCreatePixmap(it->dpy, it->window, width, height, it->visdepth);
  XSync(it->dpy, False);
  hack_resources();
  load_random_image(it->screen, it->window, pixmap, NULL, NULL);
  image = XGetImage(it->dpy, pixmap, 0, 0, width, height, ~0L, ZPixmap);
  XFreePixmap(it->dpy, pixmap);

  /* Make sure the window's background is not set to None, and get the
     grabbed bits (if any) off it as soon as possible. */
  XSetWindowBackground (it->dpy, it->window,
                        get_pixel_resource ("background", "Background",
                                            it->dpy, it->xgwa.colormap));
  XClearWindow (it->dpy, it->window);

  analogtv_setup_sync(input, 1, (random()%20)==0);
  rc=analogtv_load_ximage(it, input, image);
  if (image) XDestroyImage(image);
  XSync(it->dpy, False);
  return rc;
}

int
analogtv_load_xpm(analogtv *it, analogtv_input *input, char **xpm)
{
  Pixmap pixmap;
  XImage *image;
  int width,height;
  int rc;

  pixmap=xpm_data_to_pixmap (it->dpy, it->window, xpm,
                             &width, &height, NULL);
  image = XGetImage(it->dpy, pixmap, 0, 0, width, height, ~0L, ZPixmap);
  XFreePixmap(it->dpy, pixmap);
  rc=analogtv_load_ximage(it, input, image);
  if (image) XDestroyImage(image);
  XSync(it->dpy, False);
  return rc;
}

enum { MAX_STATIONS = 6 };
static int n_stations;
static analogtv_input *stations[MAX_STATIONS];


void add_stations(void)
{
  while (n_stations < MAX_STATIONS) {
    analogtv_input *input=analogtv_input_allocate();
    stations[n_stations++]=input;

    if (n_stations==1) {
      input->updater = update_smpte_colorbars;
      input->do_teletext=1;
    }
#ifdef USE_TEST_PATTERNS
    else if (random()%5==0) {
      j=random()%countof(test_patterns);
      analogtv_setup_sync(input);
      analogtv_load_xpm(tv, input, test_patterns[j]);
      analogtv_setup_teletext(input);
    }
#endif
    else {
      analogtv_load_random_image(tv, input);
      input->do_teletext=1;
    }
  }
}


void
screenhack (Display *dpy, Window window)
{
  int i;
  int curinputi;
  int change_ticks;
  int using_mouse=0;
  int change_now;
  chansetting chansettings[N_CHANNELS];
  chansetting *cs;
  int last_station=42;
  int delay = get_integer_resource("delay", "Integer");
  if (delay < 1) delay = 1;

  analogtv_make_font(dpy, window, &ugly_font, 7, 10, "6x10");
  
  tv=analogtv_allocate(dpy, window);
  tv->event_handler = screenhack_handle_event;

  add_stations();

  analogtv_set_defaults(tv, "");
  tv->need_clear=1;

  if (random()%4==0) {
    tv->tint_control += pow(frand(2.0)-1.0, 7) * 180.0;
  }
  if (1) {
    tv->color_control += frand(0.3);
  }

  for (i=0; i<N_CHANNELS; i++) {
    memset(&chansettings[i], 0, sizeof(chansetting));

    chansettings[i].noise_level = 0.06;
    chansettings[i].dur = 1000*delay;

    if (random()%6==0) {
      chansettings[i].dur=600;
    }
    else {
      int stati;
      for (stati=0; stati<MAX_MULTICHAN; stati++) {
        analogtv_reception *rec=&chansettings[i].recs[stati];
        int station;
        while (1) {
          station=random()%n_stations;
          if (station!=last_station) break;
          if ((random()%10)==0) break;
        }
        last_station=station;
        rec->input = stations[station];
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

  gettimeofday(&basetime,NULL);

  curinputi=0;
  cs=&chansettings[curinputi];
  change_ticks = cs->dur + 1500;

  tv->powerup=0.0;
  while (1) {
    int curticks=getticks();
    double curtime=curticks*0.001;

    change_now=0;
    if (analogtv_handle_events(tv)) {
      using_mouse=1;
      change_now=1;
    }
    if (change_now || (!using_mouse && curticks>=change_ticks 
                       && tv->powerup > 10.0)) {
      curinputi=(curinputi+1)%N_CHANNELS;
      cs=&chansettings[curinputi];
      change_ticks = curticks + cs->dur;
      /* Set channel change noise flag */
      tv->channel_change_cycles=200000;
    }

    for (i=0; i<MAX_MULTICHAN; i++) {
      analogtv_reception *rec=&cs->recs[i];
      analogtv_input *inp=rec->input;
      if (!inp) continue;

      if (inp->updater) {
        inp->next_update_time = curtime;
        (inp->updater)(inp);
      }
      rec->ofs += rec->freqerr;
    }

    tv->powerup=curtime;

    analogtv_init_signal(tv, cs->noise_level);
    for (i=0; i<MAX_MULTICHAN; i++) {
      analogtv_reception *rec=&cs->recs[i];
      analogtv_input *inp=rec->input;
      if (!inp) continue;

      analogtv_reception_update(rec);
      analogtv_add_signal(tv, rec);
    }
    analogtv_draw(tv);
  }

  XSync(dpy, False);
  XClearWindow(dpy, window);
  
  if (tv) analogtv_release(tv);
}

