/* pong, Copyright (c) 2003 Jeremy English <jenglish@myself.com>
 * A pong screen saver
 *
 * Modified by Brian Sawicki <sawicki@imsa.edu> to fix a small bug.
 * Before this fix after a certain point the paddles would be too
 * small for the program to effectively hit the ball.  The score would
 * then skyrocket as the paddles missed most every time. Added a max
 * so that once a paddle gets 10 the entire game restarts.  Special
 * thanks to Scott Zager for some help on this.
 *
 * Modified by Trevor Blackwell <tlb@tlb.org> to use analogtv.[ch] display.
 * Also added gradual acceleration of the ball, shrinking of paddles, and
 * scorekeeping.
 *
 * Modified by Gereon Steffens <gereon@steffens.org> to add -clock and -noise
 * options. See http://www.burovormkrijgers.nl (ugly flash site, 
 * navigate to Portfolio/Browse/Misc/Pong Clock) for the hardware implementation 
 * that gave me the idea. In clock mode, the score reflects the current time, and 
 * the paddles simply stop moving when it's time for the other side to score. This 
 * means that the display is only updated a few seconds *after* the minute actually 
 * changes, but I think this fuzzyness fits well with the display, and since we're
 * not displaying seconds, who cares. While I was at it, I added a -noise option
 * to control the noisyness of the display.
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
 * TLB sez: I haven't actually seen a pong game since I was about 9. Can
 * someone who has one make this look more realistic? Issues:
 *
 *  - the font for scores is wrong. For example '0' was square.
 *  - was there some kind of screen display when someone won?
 *  - did the ball move smoothly, or was the X or Y position quantized?
 *
 * It could also use better player logic: moving the paddle even when the ball
 * is going away, and making mistakes instead of just not keeping up with the
 * speeding ball.
 *
 * There is some info at http://www.mameworld.net/discrete/Atari/Atari.htm#Pong
 *
 * It says that the original Pong game did not have a microprocessor, or even a
 * custom integrated circuit. It was all discrete logic.
 *
 */

#include "screenhack.h"
#include "analogtv.h"
/* #define OUTPUT_POS */

typedef struct _paddle {
  int x;
  int y;
  int w;
  int h;
  int wait;
  int lock;
  int score;
} Paddle;

typedef struct _ball {
  int x;
  int y;
  int w;
  int h;
} Ball;

struct state {
  Display *dpy;
  Window window;

  int clock;

  Paddle l_paddle;
  Paddle r_paddle;
  Ball ball;
  int bx,by;
  int m_unit;
  int paddle_rate;
  double noise;

  analogtv *tv;
  analogtv_input *inp;
  analogtv_reception reception;

  int paddle_ntsc[4];
  int field_ntsc[4];
  int ball_ntsc[4];
  int score_ntsc[4];
  int net_ntsc[4];

  analogtv_font score_font;
};


enum {
  PONG_W = ANALOGTV_VIS_LEN,
  PONG_H = ANALOGTV_VISLINES,
  PONG_TMARG = 10
};

static void
hit_top_bottom(struct state *st)
{
  if ( (st->ball.y <= PONG_TMARG) ||
       (st->ball.y+st->ball.h >= PONG_H) )
    st->by=-st->by;
}

static void
reset_score(struct state * st)
{
  if (st->clock)
  {
    /* init score to current time */
    time_t now = time(0);
    struct tm* now_tm = localtime(&now);

    st->r_paddle.score = now_tm->tm_hour;
    st->l_paddle.score = now_tm->tm_min;
  }
  else
  {
    st->r_paddle.score = 0;
    st->l_paddle.score = 0;
  }
}

static void
new_game(struct state *st)
{
  /* Starts a Whole New Game*/
  st->ball.x = PONG_W/2;
  st->ball.y = PONG_H/2;
  st->bx = st->m_unit;
  st->by = st->m_unit;

  /* jwz: perhaps not totally accurate, but randomize things a little bit
     so that games on multiple screens are not identical. */
  if (random() & 1) st->by = -st->by;
  st->ball.y += (random() % (PONG_H/6))-(PONG_H/3);

  st->l_paddle.wait = 1;
  st->l_paddle.lock = 0;
  st->r_paddle.wait = 0;
  st->r_paddle.lock = 0;
  st->paddle_rate = st->m_unit-1;
  reset_score(st);

  st->l_paddle.h = PONG_H/4;
  st->r_paddle.h = PONG_H/4;
}

static void
start_game(struct state *st)
{
  /*Init the ball*/
  st->ball.x = PONG_W/2;
  st->ball.y = PONG_H/2;
  st->bx = st->m_unit;
  st->by = st->m_unit;

  /* jwz: perhaps not totally accurate, but randomize things a little bit
     so that games on multiple screens are not identical. */
  if (random() & 1) st->by = -st->by;
  st->ball.y += (random() % (PONG_H/6))-(PONG_H/3);

  st->l_paddle.wait = 1;
  st->l_paddle.lock = 0;
  st->r_paddle.wait = 0;
  st->r_paddle.lock = 0;
  st->paddle_rate = st->m_unit-1;

  if (st->l_paddle.h > 10) st->l_paddle.h= st->l_paddle.h*19/20;
  if (st->r_paddle.h > 10) st->r_paddle.h= st->r_paddle.h*19/20;
}

static void
hit_paddle(struct state *st)
{
  if ( st->ball.x + st->ball.w >= st->r_paddle.x &&
       st->bx > 0 ) /*we are traveling to the right*/
    {
      if ((st->ball.y + st->ball.h > st->r_paddle.y) &&
          (st->ball.y < st->r_paddle.y + st->r_paddle.h))
        {
          st->bx=-st->bx;
          st->l_paddle.wait = 0;
          st->r_paddle.wait = 1;
          st->r_paddle.lock = 0;
          st->l_paddle.lock = 0;
        }
      else
        {
          if (st->clock)
          {
            reset_score(st);
          }
          else
          {
            st->r_paddle.score++;
            if (st->r_paddle.score >=10)
              new_game(st);
            else 
              start_game(st);
          }
        }
    }

  if (st->ball.x <= st->l_paddle.x + st->l_paddle.w &&
      st->bx < 0 ) /*we are traveling to the left*/
    {
      if ( st->ball.y + st->ball.h > st->l_paddle.y &&
           st->ball.y < st->l_paddle.y + st->l_paddle.h)
        {
          st->bx=-st->bx;
          st->l_paddle.wait = 1;
          st->r_paddle.wait = 0;
          st->r_paddle.lock = 0;
          st->l_paddle.lock = 0;
        }
      else
        {
          if (st->clock)
          {
            reset_score(st);
          }
          else
          {
            st->l_paddle.score++;
            if (st->l_paddle.score >= 10)
              new_game(st);
            else
              start_game(st);
          }
        }
    }
}

static void *
pong_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));

  int i;
  struct {
    int w, h;
    char *s[10];
  } fonts[2] = { 
    { /* regular pong font */
      /* If you think we haven't learned anything since the early 70s,
         look at this font for a while */
      4, 6, 
        { 
            "****"
            "*  *"
            "*  *"
            "*  *"
            "*  *"
            "****",

            "   *"
            "   *"
            "   *"
            "   *"
            "   *"
            "   *",

            "****"
            "   *"
            "****"
            "*   "
            "*   "
            "****",

            "****"
            "   *" 
            "****"
            "   *"
            "   *"
            "****",

            "*  *"
            "*  *"
            "****"
            "   *"
            "   *"
            "   *",

            "****"
            "*   "
            "****"
            "   *"
            "   *"
            "****",

            "****"
            "*   "
            "****"
            "*  *"
            "*  *"
            "****",

            "****"
            "   *"
            "   *"
            "   *"
            "   *"
            "   *",

            "****"
            "*  *"
            "****"
            "*  *"
            "*  *"
            "****",

            "****"
            "*  *"
            "****"
            "   *"
            "   *"
            "   *"
        } 
    },
    { /* pong clock font - hand-crafted double size looks better */
      8, 12, 
        {
            "####### "
            "####### "
            "##   ## "
            "##   ## "
            "##   ## "
            "##   ## "
            "##   ## "
            "##   ## "
            "##   ## "
            "####### "
            "####### ",
            
            "   ##   "
            "   ##   "
            "   ##   "
            "   ##   "
            "   ##   "
            "   ##   "
            "   ##   "
            "   ##   "
            "   ##   "
            "   ##   "
            "   ##   ",

            "####### "
            "####### "
            "     ## "
            "     ## "
            "####### "
            "####### "
            "##      "
            "##      "
            "##      "
            "####### "
            "####### ",

            "####### "
            "####### "
            "     ## "
            "     ## "
            "####### "
            "####### "
            "     ## "
            "     ## "
            "     ## "
            "####### "
            "####### ",

            "##   ## "
            "##   ## "
            "##   ## "
            "##   ## "
            "####### "
            "####### "
            "     ## "
            "     ## "
            "     ## "
            "     ## "
            "     ## ",

            "####### "
            "####### "
            "##      "
            "##      "
            "####### "
            "####### "
            "     ## "
            "     ## "
            "     ## "
            "####### "
            "####### ",

            "####### "
            "####### "
            "##      "
            "##      "
            "####### "
            "####### "
            "##   ## "
            "##   ## "
            "##   ## "
            "####### "
            "####### ",

            "####### "
            "####### "
            "     ## "
            "     ## "
            "     ## "
            "     ## "
            "     ## "
            "     ## "
            "     ## "
            "     ## " 
            "     ## ",

            "####### "
            "####### "
            "##   ## "
            "##   ## "
            "####### "
            "####### "
            "##   ## "
            "##   ## "
            "##   ## "
            "####### "
            "####### ",

            "####### "
            "####### "
            "##   ## "
            "##   ## "
            "####### "
            "####### "
            "     ## "
            "     ## "
            "     ## "
            "####### "
            "####### "

        }
    }
  };

  st->dpy = dpy;
  st->window = window;
  st->tv=analogtv_allocate(st->dpy, st->window);
  analogtv_set_defaults(st->tv, "");


  st->clock  = get_boolean_resource(st->dpy, "clock", "Boolean");

  analogtv_make_font(st->dpy, st->window, &st->score_font, 
                     fonts[st->clock].w, fonts[st->clock].h, NULL);

  for (i=0; i<10; ++i)
  {
    analogtv_font_set_char(&st->score_font, '0'+i, fonts[st->clock].s[i]);
  }

#ifdef OUTPUT_POS
  printf("screen(%d,%d,%d,%d)\n",0,0,PONG_W,PONG_H);
#endif

  st->inp=analogtv_input_allocate();
  analogtv_setup_sync(st->inp, 0, 0);

  st->reception.input = st->inp;
  st->reception.level = 2.0;
  st->reception.ofs=0;
#if 0
  if (random()) {
    st->reception.multipath = frand(1.0);
  } else {
#endif
    st->reception.multipath=0.0;
#if 0
  }
#endif

  /*Init the paddles*/
  st->l_paddle.x = 8;
  st->l_paddle.y = 100;
  st->l_paddle.w = 16;
  st->l_paddle.h = PONG_H/4;
  st->l_paddle.wait = 1;
  st->l_paddle.lock = 0;
  st->r_paddle = st->l_paddle;
  st->r_paddle.x = PONG_W - 8 - st->r_paddle.w;
  st->r_paddle.wait = 0;
  /*Init the ball*/
  st->ball.x = PONG_W/2;
  st->ball.y = PONG_H/2;
  st->ball.w = 16;
  st->ball.h = 8;

  st->m_unit = get_integer_resource (st->dpy, "speed", "Integer");
  st->noise  = get_float_resource(st->dpy, "noise", "Float");
  st->clock  = get_boolean_resource(st->dpy, "clock", "Boolean");

  if (!st->clock)
  {
    st->score_font.y_mult *= 2;
    st->score_font.x_mult *= 2;
  }

  reset_score(st);

  start_game(st);

  analogtv_lcp_to_ntsc(ANALOGTV_BLACK_LEVEL, 0.0, 0.0, st->field_ntsc);
  analogtv_lcp_to_ntsc(100.0, 0.0, 0.0, st->ball_ntsc);
  analogtv_lcp_to_ntsc(100.0, 0.0, 0.0, st->paddle_ntsc);
  analogtv_lcp_to_ntsc(100.0, 0.0, 0.0, st->score_ntsc);
  analogtv_lcp_to_ntsc(100.0, 0.0, 0.0, st->net_ntsc);

  analogtv_draw_solid(st->inp,
                      ANALOGTV_VIS_START, ANALOGTV_VIS_END,
                      ANALOGTV_TOP, ANALOGTV_BOT,
                      st->field_ntsc);

  return st;
}

static void
p_logic(struct state *st, Paddle *p)
{
  int targ;
  if (st->bx > 0) {
    targ = st->ball.y + st->by * (st->r_paddle.x-st->ball.x) / st->bx;
  }
  else if (st->bx < 0) {
    targ = st->ball.y - st->by * (st->ball.x - st->l_paddle.x - st->l_paddle.w) / st->bx;
  }
  else {
    targ = st->ball.y;
  }
  if (targ > PONG_H) targ=PONG_H;
  if (targ < 0) targ=0;

  if (targ < p->y && !p->lock)
  {
    p->y -= st->paddle_rate;
  }
  else if (targ > (p->y + p->h) && !p->lock)
  {
    p->y += st->paddle_rate;
  }
  else
  {
    int move=targ - (p->y + p->h/2);
    if (move>st->paddle_rate) move=st->paddle_rate;
    if (move<-st->paddle_rate) move=-st->paddle_rate;
    p->y += move;
    p->lock = 1;
  }
}

static void
p_hit_top_bottom(Paddle *p)
{
  if(p->y <= PONG_TMARG)
  {
    p->y = PONG_TMARG;
  }
  if((p->y + p->h) >= PONG_H)
  {
    p->y = PONG_H - p->h;
  }
}

/*
  XFillRectangle (dpy, window, gc, p->x, p->y, p->w, p->h);
  if (old_v > p->y)
  {
    XClearArea(dpy,window, p->x, p->y + p->h,
      p->w, (old_v + p->h) - (p->y + p->h), 0);
  }
  else if (old_v < p->y)
  {
    XClearArea(dpy,window, p->x, old_v, p->w, p->y - old_v, 0);
  }
*/
static void
paint_paddle(struct state *st, Paddle *p)
{
  analogtv_draw_solid(st->inp,
                      ANALOGTV_VIS_START + p->x, ANALOGTV_VIS_START + p->x + p->w,
                      ANALOGTV_TOP, ANALOGTV_BOT,
                      st->field_ntsc);

  analogtv_draw_solid(st->inp,
                      ANALOGTV_VIS_START + p->x, ANALOGTV_VIS_START + p->x + p->w,
                      ANALOGTV_TOP + p->y, ANALOGTV_TOP + p->y + p->h,
                      st->paddle_ntsc);
}

/*
  XClearArea(dpy,window, old_ballx, old_bally, st->ball.d, st->ball.d, 0);
  XFillRectangle (dpy, window, gc, st->ball.x, st->ball.y, st->ball.d, st->ball.d);
  XFillRectangle (dpy, window, gc, xgwa.width / 2, 0, st->ball.d, xgwa.height);
*/

static void
erase_ball(struct state *st)
{
  analogtv_draw_solid(st->inp,
                      ANALOGTV_VIS_START + st->ball.x, ANALOGTV_VIS_START + st->ball.x + st->ball.w,
                      ANALOGTV_TOP + st->ball.y, ANALOGTV_TOP + st->ball.y + st->ball.h,
                      st->field_ntsc);
}

static void
paint_ball(struct state *st)
{
  analogtv_draw_solid(st->inp,
                      ANALOGTV_VIS_START + st->ball.x, ANALOGTV_VIS_START + st->ball.x + st->ball.w,
                      ANALOGTV_TOP + st->ball.y, ANALOGTV_TOP + st->ball.y + st->ball.h,
                      st->ball_ntsc);
}

static void
paint_score(struct state *st)
{
  char buf[256];

  char* fmt = (st->clock ? "%02d" : "%d");

  analogtv_draw_solid(st->inp,
                      ANALOGTV_VIS_START, ANALOGTV_VIS_END,
                      ANALOGTV_TOP, ANALOGTV_TOP + 10+ st->score_font.char_h * st->score_font.y_mult,
                      st->field_ntsc);


  sprintf(buf, fmt ,st->r_paddle.score%256);
  analogtv_draw_string(st->inp, &st->score_font, buf,
                       ANALOGTV_VIS_START + 130, ANALOGTV_TOP + 8,
                       st->score_ntsc);

  sprintf(buf, fmt, st->l_paddle.score%256);
  analogtv_draw_string(st->inp, &st->score_font, buf,
                       ANALOGTV_VIS_END - 200, ANALOGTV_TOP + 8,
                       st->score_ntsc);

}

static void
paint_net(struct state *st)
{
  int x,y;

  x=(ANALOGTV_VIS_START + ANALOGTV_VIS_END)/2;

  for (y=ANALOGTV_TOP; y<ANALOGTV_BOT; y+=6) {
    analogtv_draw_solid(st->inp, x-2, x+2, y, y+3,
                        st->net_ntsc);
    analogtv_draw_solid(st->inp, x-2, x+2, y+3, y+6,
                        st->field_ntsc);

  }
}

static unsigned long
pong_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->clock)
  {
    time_t now = time(0);
    struct tm* tm_now = localtime(&now);

    if (st->r_paddle.score != tm_now->tm_hour)
    {
      /* l paddle must score */
      st->r_paddle.wait = 1;
    }
    else if (st->l_paddle.score != tm_now->tm_min)
    {
      /* r paddle must score */
      st->l_paddle.wait = 1;
    }
  }
  erase_ball(st);

  st->ball.x += st->bx;
  st->ball.y += st->by;

  if (!st->clock)
  {
    /* in non-clock mode, occasionally increase ball speed */
    if ((random()%40)==0) {
      if (st->bx>0) st->bx++; else st->bx--;
    }
  }

  if (!st->r_paddle.wait)
  {
    p_logic(st, &st->r_paddle);
  }
  if (!st->l_paddle.wait)
  {
    p_logic(st, &st->l_paddle);
  }

  p_hit_top_bottom(&st->r_paddle);
  p_hit_top_bottom(&st->l_paddle);

  hit_top_bottom(st);
  hit_paddle(st);

  #ifdef OUTPUT_POS
  printf("(%d,%d,%d,%d)\n",st->ball.x,st->ball.y,st->ball.w,st->ball.h);
  #endif

  paint_score(st);

  paint_net(st);

  if (1) {
    paint_paddle(st, &st->r_paddle);
    paint_paddle(st, &st->l_paddle);
  }
  if (1) paint_ball(st);

  analogtv_init_signal(st->tv, st->noise);
  analogtv_reception_update(&st->reception);
  analogtv_add_signal(st->tv, &st->reception);
  analogtv_draw(st->tv);

  return 10000;
}



static const char *pong_defaults [] = {
  ".background: black",
  ".foreground: white",
  "*speed:      6",
  "*noise:      0.04",
  "*clock:      false",
  ANALOGTV_DEFAULTS
  0
};

static XrmOptionDescRec pong_options [] = {
  { "-speed",           ".speed",     XrmoptionSepArg, 0 },
  { "-noise",           ".noise",     XrmoptionSepArg, 0 },
  { "-clock",           ".clock",     XrmoptionNoArg, "true" },
  ANALOGTV_OPTIONS
  { 0, 0, 0, 0 }
};

static void
pong_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  analogtv_reconfigure (st->tv);
}

static Bool
pong_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
pong_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  analogtv_release(st->tv);
  free (st);
}

XSCREENSAVER_MODULE ("Pong", pong)
