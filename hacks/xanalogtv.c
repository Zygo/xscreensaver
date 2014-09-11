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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <math.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef HAVE_COCOA
# include <X11/Intrinsic.h> /* for XtDatabase in hack_resources() */
#endif

#include "screenhack.h"
#include "xpm-pixmap.h"
#include "analogtv.h"

#include "images/logo-50.xpm"

/* #define DEBUG 1 */
/* #define USE_TEST_PATTERNS */

#define countof(x) (sizeof((x))/sizeof((*x)))

enum {
  N_CHANNELS=12, /* Channels 2 through 13 on VHF */
  MAX_MULTICHAN=2,
  MAX_STATIONS=6
}; 

typedef struct chansetting_s {

  analogtv_reception recs[MAX_MULTICHAN];
  double noise_level;
  Bool image_loaded_p;
/*  char *filename;     was only used for diagnostics */
  int dur;
} chansetting;


struct state {
  Display *dpy;
  Window window;
  analogtv *tv;
  analogtv_font ugly_font;
  struct timeval basetime;

  int n_stations;
  analogtv_input *stations[MAX_STATIONS];
  Bool image_loading_p;

  int curinputi;
  int change_ticks;
  chansetting chansettings[N_CHANNELS];
  chansetting *cs;

  int change_now;

};


static void
update_smpte_colorbars(analogtv_input *input)
{
  struct state *st = (struct state *) input->client_data;
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

      analogtv_draw_string_centered(input, &st->ugly_font, localname,
                                    xpos, ypos, black_ntsc);
    }
  }
  ypos += st->ugly_font.char_h*5/2;

  analogtv_draw_xpm(st->tv, input,
                    logo_50_xpm, xpos - 100, ypos);

  ypos += 58;

#if 0
  analogtv_draw_string_centered(input, &st->ugly_font,
                                "Please Stand By", xpos, ypos);
  ypos += st->ugly_font.char_h*4;
#endif

  {
    char timestamp[256];
    time_t t = time ((time_t *) 0);
    struct tm *tm = localtime (&t);

    /* Y2K: It is OK for this to use a 2-digit year because it's simulating a
       TV display and is purely decorative. */
    strftime(timestamp, sizeof(timestamp)-1, "%y.%m.%d %H:%M:%S ", tm);
    analogtv_draw_string_centered(input, &st->ugly_font, timestamp,
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

static const char *xanalogtv_defaults [] = {
  ".background:	        black",
  ".foreground:	        white",
  "*delay:	        5",
  "*grabDesktopImages:  False",   /* HAVE_COCOA */
  "*chooseRandomImages: True",    /* HAVE_COCOA */
  ANALOGTV_DEFAULTS
  0,
};

static XrmOptionDescRec xanalogtv_options [] = {
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


static int
getticks(struct state *st)
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return ((tv.tv_sec - st->basetime.tv_sec)*1000 +
          (tv.tv_usec - st->basetime.tv_usec)/1000);
}


/* The first time we grab an image, do it the default way.
   The second and subsequent times, add "-no-desktop" to the command.
   That way we don't have to watch the window un-map 5+ times in a row.
   Also, we end up with the desktop on only one channel, and pictures
   on all the others (or colorbars, if no imageDirectory is set.)
 */
static void
hack_resources (Display *dpy)
{
#ifndef HAVE_COCOA
  static int count = -1;
  count++;

  if (count == 0)
    return;
  else if (count == 1)
    {
      XrmDatabase db = XtDatabase (dpy);
      char *res = "desktopGrabber";
      char *val = get_string_resource (dpy, res, "DesktopGrabber");
      char buf1[255];
      char buf2[255];
      XrmValue value;
      sprintf (buf1, "%.100s.%.100s", progname, res);
      sprintf (buf2, "%.200s -no-desktop", val);
      value.addr = buf2;
      value.size = strlen(buf2);
      XrmPutResource (&db, buf1, "String", &value);
    }
#endif /* HAVE_COCOA */
}


static void analogtv_load_random_image(struct state *);


static void image_loaded_cb (Screen *screen, Window window, Drawable pixmap,
                             const char *name, XRectangle *geometry,
                             void *closure)
{
  /* When an image has just been loaded, store it into the first available
     channel.  If there are other unloaded channels, then start loading
     another image.
  */
  struct state *st = (struct state *) closure;
  int i;
  int this = -1;
  int next = -1;

  if (!st->image_loading_p) abort();  /* only one at a time... */
  st->image_loading_p = False;

  for (i = 0; i < MAX_STATIONS; i++) {
    if (! st->chansettings[i].image_loaded_p) {
      if (this == -1) this = i;
      else if (next == -1) next = i;
    }
  }
  if (this == -1) abort();  /* no unloaded stations? */

  /* Load this image into the next channel. */
  {
    analogtv_input *input = st->stations[this];
    int width=ANALOGTV_PIC_LEN;
    int height=width*3/4;
    XImage *image = XGetImage (st->dpy, pixmap, 0, 0,
                               width, height, ~0L, ZPixmap);
    XFreePixmap(st->dpy, pixmap);

    analogtv_setup_sync(input, 1, (random()%20)==0);
    analogtv_load_ximage(st->tv, input, image);
    if (image) XDestroyImage(image);
    st->chansettings[this].image_loaded_p = True;
#if 0
    if (name) {
      const char *s = strrchr (name, '/');
      if (s) s++;
      else s = name;
      st->chansettings[this].filename = strdup (s);
    }
    fprintf(stderr, "%s: loaded channel %d, %s\n", progname, this, 
            st->chansettings[this].filename);
#endif
  }

  /* If there are still unloaded stations, fire off another loader. */
  if (next != -1)
    analogtv_load_random_image (st);
}


/* Queues a single image for loading.  Only load one at a time.
   The image is done loading when st->img_loader is null and
   it->loaded_image is a pixmap.
 */
static void
analogtv_load_random_image(struct state *st)
{
  int width=ANALOGTV_PIC_LEN;
  int height=width*3/4;
  Pixmap p;

  if (st->image_loading_p)  /* a load is already in progress */
    return;

  st->image_loading_p = True;
  p = XCreatePixmap(st->dpy, st->window, width, height, st->tv->visdepth);
  hack_resources(st->dpy);
  load_image_async (st->tv->xgwa.screen, st->window, p, image_loaded_cb, st);
}


#if 0
static int
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
  return rc;
}
#endif


static void add_stations(struct state *st)
{
  while (st->n_stations < MAX_STATIONS) {
    analogtv_input *input=analogtv_input_allocate();
    st->stations[st->n_stations++]=input;
    input->client_data = st;
  }
}


static void load_station_images(struct state *st)
{
  int i;
  for (i = 0; i < MAX_STATIONS; i++) {
    analogtv_input *input = st->stations[i];

    st->chansettings[i].image_loaded_p = True;
    if (i == 0) {   /* station 0 is always colorbars */
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
      analogtv_load_random_image(st);
      input->do_teletext=1;
      st->chansettings[i].image_loaded_p = False;
    }
  }
}


static void *
xanalogtv_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int i;
  int last_station=42;
  int delay = get_integer_resource(dpy, "delay", "Integer");
  if (delay < 1) delay = 1;

  analogtv_make_font(dpy, window, &st->ugly_font, 7, 10, "6x10");
  
  st->dpy = dpy;
  st->window = window;
  st->tv=analogtv_allocate(dpy, window);

  add_stations(st);

  analogtv_set_defaults(st->tv, "");
  st->tv->need_clear=1;

  if (random()%4==0) {
    st->tv->tint_control += pow(frand(2.0)-1.0, 7) * 180.0;
  }
  if (1) {
    st->tv->color_control += frand(0.3);
  }

  for (i=0; i<N_CHANNELS; i++) {
    memset(&st->chansettings[i], 0, sizeof(chansetting));

    st->chansettings[i].noise_level = 0.06;
    st->chansettings[i].dur = 1000*delay;

    if (random()%6==0) {
      st->chansettings[i].dur=600;
    }
    else {
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

  gettimeofday(&st->basetime,NULL);

  st->curinputi=0;
  st->cs = &st->chansettings[st->curinputi];
  st->change_ticks = st->cs->dur + 1500;

  st->tv->powerup=0.0;

  load_station_images(st);

  return st;
}

static unsigned long
xanalogtv_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;

  int curticks=getticks(st);
  double curtime=curticks*0.001;

  const analogtv_reception *recs[MAX_MULTICHAN];
  unsigned rec_count = 0;

  int auto_change = curticks >= st->change_ticks && st->tv->powerup > 10.0 ? 1 : 0;

  if (st->change_now || auto_change) {
    st->curinputi=(st->curinputi+st->change_now+auto_change+N_CHANNELS)%N_CHANNELS;
    st->change_now = 0;
    st->cs = &st->chansettings[st->curinputi];
#if 0
    fprintf (stderr, "%s: channel %d, %s\n", progname, st->curinputi,
             st->cs->filename);
#endif
    st->change_ticks = curticks + st->cs->dur;
    /* Set channel change noise flag */
    st->tv->channel_change_cycles=200000;
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

  st->tv->powerup=curtime;

  for (i=0; i<MAX_MULTICHAN; i++) {
    analogtv_reception *rec = &st->cs->recs[i];
    if (rec->input) {
      analogtv_reception_update(rec);
      recs[rec_count] = rec;
      ++rec_count;
    }
  }
  analogtv_draw(st->tv, st->cs->noise_level, recs, rec_count);

#ifdef USE_IPHONE
  return 0;
#else
  return 5000;
#endif
}

static void
xanalogtv_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  analogtv_reconfigure(st->tv);
}

static Bool
xanalogtv_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;

  if (event->type == ButtonPress)
    {
      unsigned button = event->xbutton.button;
      st->change_now = button == 2 || button == 3 || button == 5 ? -1 : 1;
      return True;
    }
  else if (event->type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
          keysym == XK_Up || keysym == XK_Right || keysym == XK_Prior)
        {
          st->change_now = 1;
          return True;
        }
      else if (c == '\b' ||
               keysym == XK_Down || keysym == XK_Left || keysym == XK_Next)
        {
          st->change_now = -1;
          return True;
        }
      else if (screenhack_event_helper (dpy, window, event))
        goto DEF;
    }
  else if (screenhack_event_helper (dpy, window, event))
    {
    DEF:
      st->change_now = ((random() & 1) ? 1 : -1);
      return True;
    }

  return False;
}

static void
xanalogtv_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  analogtv_release(st->tv);
  free (st);
}


XSCREENSAVER_MODULE ("XAnalogTV", xanalogtv)
