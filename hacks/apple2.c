/* xscreensaver, Copyright (c) 1998-2004 Jamie Zawinski <jwz@jwz.org>
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
 */

#include <math.h>
#include "screenhack.h"
#include "apple2.h"
#include <time.h>
#include <sys/time.h>
#include <X11/Intrinsic.h>

#ifdef HAVE_XSHM_EXTENSION
#include "xshm.h"
#endif

#define DEBUG

extern XtAppContext app;

/*
 * Implementation notes
 *
 * The A2 had 3 display modes: text, lores, and hires. Text was 40x24, and it
 * disabled color in the TV. Lores gave you 40x48 graphics blocks, using the
 * same memory as the text screen. Each could be one of 16 colors. Hires gave
 * you 280x192 pixels. Odd pixels were blue or purple, and even pixels were
 * orange or green depending on the setting of the high bit in each byte.
 *
 * The graphics modes could also have 4 lines of text at the bottom. This was
 * fairly unreadable if you had a color monitor.
 *
 * Each mode had 2 different screens using different memory space. In hires
 * mode this was sometimes used for double buffering, but more often the lower
 * screen was full of code/data and the upper screen was used for display, so
 * you got random garbage on the screen.
 *
 * The text font is based on X's standard 6x10 font, with a few tweaks like
 * putting a slash across the zero.
 *
 * To use this, you'll call apple2(display, window, duration,
 * controller) where the function controller defines what will happen.
 * See bsod.c and apple2-main.c for example controllers. The
 * controller function gets called whenever the machine ready to start
 * something new. By setting sim->printing or sim->typing, it'll be
 * busy for some time spitting characters out one at a time. By
 * setting *next_actiontime+=X.X, it'll pause and just update the screen
 * for that long before calling the controller function again.
 *
 * By setting stepno to A2CONTROLLER_DONE, the loop will end. It will also end
 * after the time specified by the delay parameter. In either case, it calls
 * the controller with stepno==A2CONTROLLER_FREE to allow it to release any
 * memory.
 *
 * The void* apple2_sim_t::controller_data is for the use of the controller.
 * It will be initialize to NULL, and the controller can store its own state
 * there.
 *
 */

void
a2_scroll(apple2_state_t *st)
{
  int i;
  st->textlines[st->cursy][st->cursx] |= 0xc0;     /* turn off cursor */

  for (i=0; i<23; i++) {
    memcpy(st->textlines[i],st->textlines[i+1],40);
  }
  memset(st->textlines[23],0xe0,40);
}

static void
a2_printc_1(apple2_state_t *st, char c, int scroll_p)
{
  st->textlines[st->cursy][st->cursx] |= 0xc0; /* turn off blink */

  if (c == '\n')                      /* ^J == NL */
    {
      if (st->cursy==23)
        {
          if (scroll_p)
            a2_scroll(st);
        }
      else
        {
          st->cursy++;
        }
      st->cursx=0;
    }
  else if (c == 014)                  /* ^L == CLS, Home */
    {
      a2_cls(st);
      a2_goto(st,0,0);
    }
  else if (c == '\t')                 /* ^I == tab */
    {
      a2_goto(st, st->cursy, (st->cursx+8)&~7);
    }
  else if (c == 010)                  /* ^H == backspace */
    {
      st->textlines[st->cursy][st->cursx]=0xe0;
      a2_goto(st, st->cursy, st->cursx-1);
    }
  else if (c == '\r')                 /* ^M == CR */
    {
      st->cursx=0;
    }
  else
    {
      st->textlines[st->cursy][st->cursx]=c ^ 0xc0;
      st->cursx++;
      if (st->cursx==40) {
        if (st->cursy==23) {
          if (scroll_p)
            a2_scroll(st);
        } else {
          st->cursy++;
        }
        st->cursx=0;
      }
    }

  st->textlines[st->cursy][st->cursx] &= 0x7f; /* turn on blink */
}

void
a2_printc(apple2_state_t *st, char c)
{
  a2_printc_1(st, c, 1);
}

void
a2_printc_noscroll(apple2_state_t *st, char c)
{
  a2_printc_1(st, c, 0);
}


void
a2_prints(apple2_state_t *st, char *s)
{
  while (*s) a2_printc(st, *s++);
}

void
a2_goto(apple2_state_t *st, int r, int c)
{
  if (r > 23) r = 23;
  if (c > 39) r = 39;
  st->textlines[st->cursy][st->cursx] |= 0xc0; /* turn off blink */
  st->cursy=r;
  st->cursx=c;
  st->textlines[st->cursy][st->cursx] &= 0x7f; /* turn on blink */
}

void
a2_cls(apple2_state_t *st)
{
  int i;
  for (i=0; i<24; i++) {
    memset(st->textlines[i],0xe0,40);
  }
}

void
a2_clear_gr(apple2_state_t *st)
{
  int i;
  for (i=0; i<24; i++) {
    memset(st->textlines[i],0x00,40);
  }
}

void
a2_clear_hgr(apple2_state_t *st)
{
  int i;
  for (i=0; i<192; i++) {
    memset(st->hireslines[i],0,40);
  }
}

void
a2_invalidate(apple2_state_t *st)
{
}

void
a2_poke(apple2_state_t *st, int addr, int val)
{

  if (addr>=0x400 && addr<0x800) {
    /* text memory */
    int row=((addr&0x380)/0x80) + ((addr&0x7f)/0x28)*8;
    int col=(addr&0x7f)%0x28;
    if (row<24 && col<40) {
      st->textlines[row][col]=val;
      if (!(st->gr_mode&(A2_GR_HIRES)) ||
          (!(st->gr_mode&(A2_GR_FULL)) && row>=20)) {
      }
    }
  }
  else if (addr>=0x2000 && addr<0x4000) {
    int row=(((addr&0x1c00) / 0x400) * 1 +
             ((addr&0x0380) / 0x80) * 8 +
             ((addr&0x0078) / 0x28) * 64);
    int col=((addr&0x07f)%0x28);
    if (row<192 && col<40) {
      st->hireslines[row][col]=val;
      if (st->gr_mode&A2_GR_HIRES) {
      }
    }
  }
}

void
a2_hplot(apple2_state_t *st, int hcolor, int x, int y)
{
  int highbit,run;

  highbit=((hcolor<<5)&0x80) ^ 0x80; /* capture bit 2 into bit 7 */

  if (y<0 || y>=192 || x<0 || x>=280) return;

  for (run=0; run<2 && x<280; run++) {
    u_char *vidbyte = &st->hireslines[y][x/7];
    u_char whichbit=1<<(x%7);
    int masked_bit;

    *vidbyte = (*vidbyte & 0x7f) | highbit;

    /* use either bit 0 or 1 of hcolor for odd or even pixels */
    masked_bit = (hcolor>>(1-(x&1)))&1;

    /* Set whichbit to 1 or 0 depending on color */
    *vidbyte = (*vidbyte & ~whichbit) | (masked_bit ? whichbit : 0);

    x++;
  }
}

void
a2_hline(apple2_state_t *st, int hcolor, int x1, int y1, int x2, int y2)
{
  int dx,dy,incx,incy,x,y,balance;

  /* Bresenham's line drawing algorithm */

  if (x2>=x1) {
    dx=x2-x1;
    incx=1;
  } else {
    dx=x1-x2;
    incx=-1;
  }
  if (y2>=y1) {
    dy=y2-y1;
    incy=1;
  } else {
    dy=y1-y2;
    incy=-1;
  }

  x=x1; y=y1;

  if (dx>=dy) {
    dy*=2;
    balance=dy-dx;
    dx*=2;
    while (x!=x2) {
      a2_hplot(st, hcolor, x, y);
      if (balance>=0) {
        y+=incy;
        balance-=dx;
      }
      balance+=dy;
      x+=incx;
    }
    a2_hplot(st, hcolor, x, y);
  } else {
    dx*=2;
    balance=dx-dy;
    dy*=2;
    while (y!=y2) {
      a2_hplot(st, hcolor, x, y);
      if (balance>=0) {
        x+=incx;
        balance-=dy;
      }
      balance+=dx;
      y+=incy;
    }
    a2_hplot(st, hcolor, x, y);
  }
}

void
a2_plot(apple2_state_t *st, int color, int x, int y)
{
  int textrow=y/2;
  u_char byte;

  if (x<0 || x>=40 || y<0 || y>=48) return;

  byte=st->textlines[textrow][x];
  if (y&1) {
    byte = (byte&0xf0) | (color&0x0f);
  } else {
    byte = (byte&0x0f) | ((color&0x0f)<<4);
  }
  st->textlines[textrow][x]=byte;
}

void
a2_display_image_loading(apple2_state_t *st, unsigned char *image,
                         int lineno)
{
  /*
    When loading images,it would normally just load the big binary
    dump into screen memory while you watched. Because of the way
    screen memory was laid out, it wouldn't load from the top down,
    but in a funny interleaved way. You should call this with lineno
    increasing from 0 thru 191 over a period of a few seconds.
  */

  int row=(((lineno / 24) % 8) * 1 +
           ((lineno / 3 ) % 8) * 8 +
           ((lineno / 1 ) % 3) * 64);

  memcpy (st->hireslines[row], &image[row * 40], 40);
}

/*
  Simulate plausible initial memory contents for running a program.
*/
void
a2_init_memory_active(apple2_sim_t *sim)
{
  int i,j,x,y,c;
  int addr=0;
  apple2_state_t *st=sim->st;

  while (addr<0x4000) {
    int n;

    switch (random()%4) {
    case 0:
    case 1:
      n=random()%500;
      for (i=0; i<n && addr<0x4000; i++) {
        u_char rb=((random()%6==0 ? 0 : random()%16) |
                   ((random()%5==0 ? 0 : random()%16)<<4));
        a2_poke(st, addr++, rb);
      }
      break;

    case 2:
      /* Simulate shapes stored in memory. We use the font since we have it.
         Unreadable, since rows of each character are stored in consecutive
         bytes. It was typical to store each of the 7 possible shifts of
         bitmaps, for fastest blitting to the screen. */
      x=random()%(sim->text_im->width);
      for (i=0; i<100; i++) {
        for (y=0; y<8; y++) {
          c=0;
          for (j=0; j<8; j++) {
            c |= XGetPixel(sim->text_im, (x+j)%sim->text_im->width, y)<<j;
          }
          a2_poke(st, addr++, c);
        }
        x=(x+1)%(sim->text_im->width);
      }
      break;

    case 3:
      if (addr>0x2000) {
        n=random()%200;
        for (i=0; i<n && addr<0x4000; i++) {
          a2_poke(st, addr++, 0);
        }
      }
      break;

    }
  }
}


/* This table lists fixes for characters that differ from the standard 6x10
   font. Each encodes a pixel, as (charindex*7 + x) + (y<<10) + (value<<15)
   where value is 0 for white and 1 for black. */
static unsigned short a2_fixfont[] = {
  /* Fix $ */  0x8421, 0x941d,
  /* Fix % */  0x8024, 0x0028, 0x8425, 0x0426, 0x0825, 0x1027, 0x1426, 0x9427,
               0x1824, 0x9828,
  /* Fix * */  0x8049, 0x8449, 0x8849, 0x0c47, 0x0c48, 0x0c4a, 0x0c4b, 0x9049,
               0x9449, 0x9849,
  /* Fix , */  0x9057, 0x1458, 0x9856, 0x1857, 0x1c56,
  /* Fix . */  0x1465, 0x1864, 0x1866, 0x1c65,
  /* Fix / */  0x006e, 0x186a,
  /* Fix 0 */  0x8874, 0x8c73, 0x9072,
  /* Fix 1 */  0x0878, 0x1878, 0x187c,
  /* Fix 5 */  0x8895, 0x0c94, 0x0c95,
  /* Fix 6 */  0x809f, 0x8c9c, 0x109c,
  /* Fix 7 */  0x8ca4, 0x0ca5, 0x90a3, 0x10a4,
  /* Fix 9 */  0x08b3, 0x8cb3, 0x98b0,
  /* Fix : */  0x04b9, 0x08b8, 0x08ba, 0x0cb9, 0x90b9, 0x14b9, 0x18b8, 0x18b9,
               0x18ba, 0x1cb9,
  /* Fix ; */  0x04c0, 0x08bf, 0x08c1, 0x0cc0, 0x90c0, 0x14c1, 0x98bf, 0x18c0,
               0x1cbf,
  /* Fix < */  0x80c8, 0x00c9, 0x84c7, 0x04c8, 0x88c6, 0x08c7, 0x8cc5, 0x0cc6,
               0x90c6, 0x10c7,
               0x94c7, 0x14c8, 0x98c8, 0x18c9,
  /* Fix > */  0x80d3, 0x00d4, 0x84d4, 0x04d5, 0x88d5, 0x08d6, 0x8cd6, 0x0cd7,
               0x90d5, 0x10d6,
               0x94d4, 0x14d5, 0x98d3, 0x18d4,
  /* Fix @ */  0x88e3, 0x08e4, 0x8ce4, 0x98e5,
  /* Fix B */  0x84ef, 0x04f0, 0x88ef, 0x08f0, 0x8cef, 0x90ef, 0x10f0, 0x94ef,
               0x14f0,
  /* Fix D */  0x84fd, 0x04fe, 0x88fd, 0x08fe, 0x8cfd, 0x0cfe, 0x90fd, 0x10fe,
               0x94fd, 0x14fe,
  /* Fix G */  0x8116, 0x0516, 0x9916,
  /* Fix J */  0x0129, 0x012a, 0x052a, 0x852b, 0x092a, 0x892b, 0x0d2a, 0x8d2b,
               0x112a, 0x912b,
               0x152a, 0x952b, 0x992a,
  /* Fix M */  0x853d, 0x853f, 0x093d, 0x893e, 0x093f,
  /* Fix Q */  0x915a, 0x155a, 0x955b, 0x155c, 0x195b, 0x995c, 0x1d5c,
  /* Fix V */  0x8d7b, 0x0d7c, 0x0d7e, 0x8d7f, 0x917b, 0x117c, 0x117e, 0x917f,
  /* Fix [ */  0x819e, 0x81a2, 0x859e, 0x899e, 0x8d9e, 0x919e, 0x959e, 0x999e,
               0x99a2,
  /* Fix \ */  0x01a5, 0x19a9,
  /* Fix ] */  0x81ac, 0x81b0, 0x85b0, 0x89b0, 0x8db0, 0x91b0, 0x95b0, 0x99ac,
               0x99b0,
  /* Fix ^ */  0x01b5, 0x05b4, 0x05b6, 0x09b3, 0x89b5, 0x09b7, 0x8db4, 0x8db6,
               0x91b3, 0x91b7,
  /* Fix _ */  0x9db9, 0x9dbf,
  0,
};

static void
a2_make_font(apple2_sim_t *sim)
{
  /*
    Generate the font. It used a 5x7 font which looks a lot like the standard X
    6x10 font, with a few differences. So we render up all the uppercase
    letters of 6x10, and make a few tweaks (like putting a slash across the
    zero) according to fixfont.
   */

  int i;
  const char *def_font="6x10";
  XFontStruct *font;
  Pixmap text_pm;
  GC gc;
  XGCValues gcv;

  font = XLoadQueryFont (sim->dpy, def_font);
  if (!font) {
    fprintf(stderr, "%s: can't load font %s\n", progname, def_font);
    abort();
  }

  text_pm=XCreatePixmap(sim->dpy, sim->window, 64*7, 8, sim->dec->xgwa.depth);

  memset(&gcv, 0, sizeof(gcv));
  gcv.foreground=1;
  gcv.background=0;
  gcv.font=font->fid;
  gc=XCreateGC(sim->dpy, text_pm, GCFont|GCBackground|GCForeground, &gcv);

  XSetForeground(sim->dpy, gc, 0);
  XFillRectangle(sim->dpy, text_pm, gc, 0, 0, 64*7, 8);
  XSetForeground(sim->dpy, gc, 1);
  for (i=0; i<64; i++) {
    char c=32+i;
    int x=7*i+1;
    int y=7;
    if (c=='0') {
      c='O';
      XDrawString(sim->dpy, text_pm, gc, x, y, &c, 1);
    } else {
      XDrawString(sim->dpy, text_pm, gc, x, y, &c, 1);
    }
  }
  sim->text_im = XGetImage(sim->dpy, text_pm, 0, 0, 64*7, 8, ~0L, ZPixmap);
  XFreeGC(sim->dpy, gc);
  XFreePixmap(sim->dpy, text_pm);

  for (i=0; a2_fixfont[i]; i++) {
    XPutPixel(sim->text_im, a2_fixfont[i]&0x3ff,
              (a2_fixfont[i]>>10)&0xf,
              (a2_fixfont[i]>>15)&1);
  }
}


void
apple2(Display *dpy, Window window, int delay,
       void (*controller)(apple2_sim_t *sim,
                          int *stepno,
                          double *next_actiontime))
{
  int i,textrow,row,col,stepno;
  int c;
  double next_actiontime;
  apple2_sim_t *sim;

  sim=(apple2_sim_t *)calloc(1,sizeof(apple2_state_t));
  sim->dpy = dpy;
  sim->window = window;
  sim->delay = delay;

  sim->st = (apple2_state_t *)calloc(1,sizeof(apple2_state_t));
  sim->dec = analogtv_allocate(dpy, window);
  sim->dec->event_handler = screenhack_handle_event;
  sim->inp = analogtv_input_allocate();

  sim->reception.input = sim->inp;
  sim->reception.level = 1.0;

  sim->prompt=']';

  if (random()%4==0 && !sim->dec->use_cmap && sim->dec->use_color && sim->dec->visbits>=8) {
    sim->dec->flutter_tint=1;
  }
  else if (random()%3==0) {
    sim->dec->flutter_horiz_desync=1;
  }
  sim->typing_rate = 1.0;

  analogtv_set_defaults(sim->dec, "");
  sim->dec->squish_control=0.05;
  analogtv_setup_sync(sim->inp, 1, 0);


  a2_make_font(sim);

  stepno=0;
  a2_goto(sim->st,23,0);

  if (random()%2==0) sim->basetime_tv.tv_sec -= 1; /* random blink phase */
  next_actiontime=0.0;

  sim->curtime=0.0;
  next_actiontime=sim->curtime;
  (*controller)(sim, &stepno, &next_actiontime);

# ifdef GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&sim->basetime_tv, NULL);
# else
  gettimeofday(&sim->basetime_tv);
# endif

  while (1) {
    double blinkphase;

    {
      struct timeval curtime_tv;
# ifdef GETTIMEOFDAY_TWO_ARGS
      struct timezone tzp;
      gettimeofday(&curtime_tv, &tzp);
# else
      gettimeofday(&curtime_tv);
# endif
      sim->curtime=(curtime_tv.tv_sec - sim->basetime_tv.tv_sec) +
        0.000001*(curtime_tv.tv_usec - sim->basetime_tv.tv_usec);
      if (sim->curtime > sim->dec->powerup)
        sim->dec->powerup=sim->curtime;
    }

    if (analogtv_handle_events(sim->dec)) {
      sim->typing=NULL;
      sim->printing=NULL;
      stepno=A2CONTROLLER_FREE;
      next_actiontime = sim->curtime;
      (*controller)(sim, &stepno, &next_actiontime);
      stepno=0;
      sim->controller_data=NULL;
      sim->st->gr_mode=0;
      continue;
    }

    blinkphase=sim->curtime/0.8;

    /* The blinking rate was controlled by 555 timer with a resistor/capacitor
       time constant. Because the capacitor was electrolytic, the flash rate
       varied somewhat between machines. I'm guessing 1.6 seconds/cycle was
       reasonable. (I soldered a resistor in mine to make it blink faster.) */
    i=sim->st->blink;
    sim->st->blink=((int)blinkphase)&1;
    if (sim->st->blink!=i && !(sim->st->gr_mode&A2_GR_FULL)) {
      int downcounter=0;
      /* For every row with blinking text, set the changed flag. This basically
         works great except with random screen garbage in text mode, when we
         end up redrawing the whole screen every second */
      for (row=(sim->st->gr_mode ? 20 : 0); row<24; row++) {
        for (col=0; col<40; col++) {
          c=sim->st->textlines[row][col];
          if ((c & 0xc0) == 0x40) {
            downcounter=4;
            break;
          }
        }
        if (downcounter>0) {
          downcounter--;
        }
      }
    }

    if (sim->printing) {
      int nlcnt=0;
      while (*sim->printing) {
        if (*sim->printing=='\001') { /* pause */
          sim->printing++;
          break;
        }
        else if (*sim->printing=='\n') {
          a2_printc(sim->st,*sim->printing);
          sim->printing++;
          nlcnt++;
          if (nlcnt>=2) break;
        }
        else {
          a2_printc(sim->st,*sim->printing);
          sim->printing++;
        }
      }
      if (!*sim->printing) sim->printing=NULL;
    }
    else if (sim->curtime >= next_actiontime) {
      if (sim->typing) {

        int c;
        /* If we're in the midst of typing a string, emit a character with
           random timing. */
        c =*sim->typing++;
        if (c==0) {
          sim->typing=NULL;
        }
        else {
          a2_printc(sim->st, c);
          if (c=='\r' || c=='\n') {
            next_actiontime = sim->curtime;
          }
          else if (c==010) {
            next_actiontime = sim->curtime + 0.1;
          }
          else {
            next_actiontime = (sim->curtime +
                               (((random()%1000)*0.001 + 0.3) *
                                sim->typing_rate));
          }
        }
      } else {
        next_actiontime=sim->curtime;

        (*controller)(sim, &stepno, &next_actiontime);
        if (stepno==A2CONTROLLER_DONE) goto finished;

      }
    }


    analogtv_setup_sync(sim->inp, sim->st->gr_mode? 1 : 0, 0);
    analogtv_setup_frame(sim->dec);

    for (textrow=0; textrow<24; textrow++) {
      for (row=textrow*8; row<textrow*8+8; row++) {

        /* First we generate the pattern that the video circuitry shifts out
           of memory. It has a 14.something MHz dot clock, equal to 4 times
           the color burst frequency. So each group of 4 bits defines a color.
           Each character position, or byte in hires, defines 14 dots, so odd
           and even bytes have different color spaces. So, pattern[0..600]
           gets the dots for one scan line. */

        char *pp=&sim->inp->signal[row+ANALOGTV_TOP+4][ANALOGTV_PIC_START+100];

        if ((sim->st->gr_mode&A2_GR_HIRES) &&
            (row<160 || (sim->st->gr_mode&A2_GR_FULL))) {

          /* Emulate the mysterious pink line, due to a bit getting
             stuck in a shift register between the end of the last
             row and the beginning of this one. */
          if ((sim->st->hireslines[row][0] & 0x80) &&
              (sim->st->hireslines[row][39]&0x40)) {
            pp[-1]=ANALOGTV_WHITE_LEVEL;
          }

          for (col=0; col<40; col++) {
            u_char b=sim->st->hireslines[row][col];
            int shift=(b&0x80)?0:1;

            /* Each of the low 7 bits in hires mode corresponded to 2 dot
               clocks, shifted by one if the high bit was set. */
            for (i=0; i<7; i++) {
              pp[shift+1] = pp[shift] = (((b>>i)&1)
                                         ?ANALOGTV_WHITE_LEVEL
                                         :ANALOGTV_BLACK_LEVEL);
              pp+=2;
            }
          }
        }
        else if ((sim->st->gr_mode&A2_GR_LORES) &&
                 (row<160 || (sim->st->gr_mode&A2_GR_FULL))) {
          for (col=0; col<40; col++) {
            u_char nib=((sim->st->textlines[textrow][col] >> (((row/4)&1)*4))
                        & 0xf);
            /* The low or high nybble was shifted out one bit at a time. */
            for (i=0; i<14; i++) {
              *pp = (((nib>>((col*14+i)&3))&1)
                     ?ANALOGTV_WHITE_LEVEL
                     :ANALOGTV_BLACK_LEVEL);
              pp++;
            }
          }
        }
        else {
          for (col=0; col<40; col++) {
            int rev;
            c=sim->st->textlines[textrow][col]&0xff;
            /* hi bits control inverse/blink as follows:
               0x00: inverse
               0x40: blink
               0x80: normal
               0xc0: normal */
            rev=!(c&0x80) && (!(c&0x40) || sim->st->blink);

            for (i=0; i<7; i++) {
              unsigned long pix=XGetPixel(sim->text_im,
                                          ((c&0x3f)^0x20)*7+i,
                                          row%8);
              pp[1] = pp[2] = ((pix^rev)
                               ?ANALOGTV_WHITE_LEVEL
                               :ANALOGTV_BLACK_LEVEL);
              pp+=2;
            }
          }
        }
      }
    }
    analogtv_init_signal(sim->dec, 0.02);
    analogtv_reception_update(&sim->reception);
    analogtv_add_signal(sim->dec, &sim->reception);
    analogtv_draw(sim->dec);
  }

 finished:

  stepno=A2CONTROLLER_FREE;
  (*controller)(sim, &stepno, &next_actiontime);

  XSync(sim->dpy, False);
  XClearWindow(sim->dpy, sim->window);
}

void
a2controller_test(apple2_sim_t *sim, int *stepno, double *next_actiontime)
{
  int row,col;

  switch(*stepno) {
  case 0:
    a2_invalidate(sim->st);
    /*
      For testing color rendering. The spec is:
      red grn blu
      0  black       0   0   0
      1  red       227  30  96
      2  dk blue    96  78 189
      3  purple    255  68 253
      4  dk green    0 163  96
      5  gray      156 156 156
      6  med blue   20 207 253
      7  lt blue   208 195 255
      8  brown      96 114   3
      9  orange    255 106  60
      10 grey      156 156 156
      11 pink      255 160 208
      12 lt green   20 245  60
      13 yellow    208 221 141
      14 aqua      114 255 208
      15 white     255 255 255
    */
    sim->st->gr_mode=A2_GR_LORES;
    for (row=0; row<24; row++) {
      for (col=0; col<40; col++) {
        sim->st->textlines[row][col]=(row&15)*17;
      }
    }
    *next_actiontime+=0.4;
    *stepno=99;
    break;

  case 99:
    if (sim->curtime > 10) *stepno=-1;
    break;
  }
}
