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

Paddle l_paddle;
Paddle r_paddle;
Ball ball;
static int bx,by;
static int m_unit;
static int paddle_rate;

static analogtv *tv;
static analogtv_input *inp;
static analogtv_reception reception;

static int paddle_ntsc[4];
static int field_ntsc[4];
static int ball_ntsc[4];
static int score_ntsc[4];
static int net_ntsc[4];

analogtv_font score_font;

enum {
  PONG_W = ANALOGTV_VIS_LEN,
  PONG_H = ANALOGTV_VISLINES,
  PONG_TMARG = 10
};

static void
hit_top_bottom(void)
{
  if ( (ball.y <= PONG_TMARG) ||
       (ball.y+ball.h >= PONG_H) )
    by=-by;
}

void
new_game(void)
{
  /* Starts a Whole New Game*/
  ball.x = PONG_W/2;
  ball.y = PONG_H/2;
  bx = m_unit;
  by = m_unit;

  l_paddle.wait = 1;
  l_paddle.lock = 0;
  r_paddle.wait = 0;
  r_paddle.lock = 0;
  paddle_rate = m_unit-1;
  r_paddle.score = 0;
  l_paddle.score = 0;

  l_paddle.h = PONG_H/4;
  r_paddle.h = PONG_H/4;
}

void
start_game(void)
{
  /*Init the ball*/
  ball.x = PONG_W/2;
  ball.y = PONG_H/2;
  bx = m_unit;
  by = m_unit;

  l_paddle.wait = 1;
  l_paddle.lock = 0;
  r_paddle.wait = 0;
  r_paddle.lock = 0;
  paddle_rate = m_unit-1;

  if (l_paddle.h > 10) l_paddle.h= l_paddle.h*19/20;
  if (r_paddle.h > 10) r_paddle.h= r_paddle.h*19/20;
}

static void
hit_paddle(void)
{
  if ( ball.x + ball.w >= r_paddle.x &&
       bx > 0 ) /*we are traveling to the right*/
    {
      if ((ball.y + ball.h > r_paddle.y) &&
          (ball.y < r_paddle.y + r_paddle.h))
        {
          bx=-bx;
          l_paddle.wait = 0;
          r_paddle.wait = 1;
          r_paddle.lock = 0;
          l_paddle.lock = 0;
        }
      else
        {
          r_paddle.score++;
          if (r_paddle.score >=10)
                new_game();
          else 
          start_game();
        }
    }

  if (ball.x <= l_paddle.x + l_paddle.w &&
      bx < 0 ) /*we are traveling to the left*/
    {
      if ( ball.y + ball.h > l_paddle.y &&
           ball.y < l_paddle.y + l_paddle.h)
        {
          bx=-bx;
          l_paddle.wait = 1;
          r_paddle.wait = 0;
          r_paddle.lock = 0;
          l_paddle.lock = 0;
        }
      else
        {
          l_paddle.score++;
          if (l_paddle.score >= 10)
                new_game();
          else
          start_game();
        }
    }
}

static void
init_pong (Display *dpy, Window window)
{
  tv=analogtv_allocate(dpy, window);
  analogtv_set_defaults(tv, "");
  tv->event_handler = screenhack_handle_event;

  analogtv_make_font(dpy, window, &score_font,
                     4, 6, NULL );

  /* If you think we haven't learned anything since the early 70s,
     look at this font for a while */
  analogtv_font_set_char(&score_font, '0',
                        "****"
                        "*  *"
                        "*  *"
                        "*  *"
                        "*  *"
                        "****");
  analogtv_font_set_char(&score_font, '1',
                        "   *"
                        "   *"
                        "   *"
                        "   *"
                        "   *"
                        "   *");
  analogtv_font_set_char(&score_font, '2',
                        "****"
                        "   *"
                        "****"
                        "*   "
                        "*   "
                        "****");
  analogtv_font_set_char(&score_font, '3',
                        "****"
                        "   *"
                        "****"
                        "   *"
                        "   *"
                        "****");
  analogtv_font_set_char(&score_font, '4',
                        "*  *"
                        "*  *"
                        "****"
                        "   *"
                        "   *"
                        "   *");
  analogtv_font_set_char(&score_font, '5',
                        "****"
                        "*   "
                        "****"
                        "   *"
                        "   *"
                        "****");
  analogtv_font_set_char(&score_font, '6',
                        "****"
                        "*   "
                        "****"
                        "*  *"
                        "*  *"
                        "****");
  analogtv_font_set_char(&score_font, '7',
                        "****"
                        "   *"
                        "   *"
                        "   *"
                        "   *"
                        "   *");
  analogtv_font_set_char(&score_font, '8',
                        "****"
                        "*  *"
                        "****"
                        "*  *"
                        "*  *"
                        "****");
  analogtv_font_set_char(&score_font, '9',
                        "****"
                        "*  *"
                        "****"
                        "   *"
                        "   *"
                        "   *");

  score_font.y_mult *= 2;
  score_font.x_mult *= 2;

#ifdef OUTPUT_POS
  printf("screen(%d,%d,%d,%d)\n",0,0,PONG_W,PONG_H);
#endif

  inp=analogtv_input_allocate();
  analogtv_setup_sync(inp, 0, 0);

  reception.input = inp;
  reception.level = 2.0;
  reception.ofs=0;
#if 0
  if (random()) {
    reception.multipath = frand(1.0);
  } else {
#endif
    reception.multipath=0.0;
#if 0
  }
#endif

  /*Init the paddles*/
  l_paddle.x = 8;
  l_paddle.y = 100;
  l_paddle.w = 16;
  l_paddle.h = PONG_H/4;
  l_paddle.wait = 1;
  l_paddle.lock = 0;
  r_paddle = l_paddle;
  r_paddle.x = PONG_W - 8 - r_paddle.w;
  r_paddle.wait = 0;
  /*Init the ball*/
  ball.x = PONG_W/2;
  ball.y = PONG_H/2;
  ball.w = 16;
  ball.h = 8;

  m_unit = get_integer_resource ("speed", "Integer");

  start_game();

  analogtv_lcp_to_ntsc(ANALOGTV_BLACK_LEVEL, 0.0, 0.0, field_ntsc);
  analogtv_lcp_to_ntsc(100.0, 0.0, 0.0, ball_ntsc);
  analogtv_lcp_to_ntsc(100.0, 0.0, 0.0, paddle_ntsc);
  analogtv_lcp_to_ntsc(100.0, 0.0, 0.0, score_ntsc);
  analogtv_lcp_to_ntsc(100.0, 0.0, 0.0, net_ntsc);

  analogtv_draw_solid(inp,
                      ANALOGTV_VIS_START, ANALOGTV_VIS_END,
                      ANALOGTV_TOP, ANALOGTV_BOT,
                      field_ntsc);
}

static void
p_logic(Paddle *p)
{
  int targ;
  if (bx > 0) {
    targ = ball.y + by * (r_paddle.x-ball.x) / bx;
  }
  else if (bx < 0) {
    targ = ball.y - by * (ball.x - l_paddle.x - l_paddle.w) / bx;
  }
  else {
    targ = ball.y;
  }
  if (targ > PONG_H) targ=PONG_H;
  if (targ < 0) targ=0;

  if (targ < p->y && !p->lock)
  {
    p->y -= paddle_rate;
  }
  else if (targ > (p->y + p->h) && !p->lock)
  {
    p->y += paddle_rate;
  }
  else
  {
    int move=targ - (p->y + p->h/2);
    if (move>paddle_rate) move=paddle_rate;
    if (move<-paddle_rate) move=-paddle_rate;
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
paint_paddle(analogtv_input *inp, Paddle *p)
{
  analogtv_draw_solid(inp,
                      ANALOGTV_VIS_START + p->x, ANALOGTV_VIS_START + p->x + p->w,
                      ANALOGTV_TOP, ANALOGTV_BOT,
                      field_ntsc);

  analogtv_draw_solid(inp,
                      ANALOGTV_VIS_START + p->x, ANALOGTV_VIS_START + p->x + p->w,
                      ANALOGTV_TOP + p->y, ANALOGTV_TOP + p->y + p->h,
                      paddle_ntsc);
}

/*
  XClearArea(dpy,window, old_ballx, old_bally, ball.d, ball.d, 0);
  XFillRectangle (dpy, window, gc, ball.x, ball.y, ball.d, ball.d);
  XFillRectangle (dpy, window, gc, xgwa.width / 2, 0, ball.d, xgwa.height);
*/

static void
erase_ball(analogtv_input *inp)
{
  analogtv_draw_solid(inp,
                      ANALOGTV_VIS_START + ball.x, ANALOGTV_VIS_START + ball.x + ball.w,
                      ANALOGTV_TOP + ball.y, ANALOGTV_TOP + ball.y + ball.h,
                      field_ntsc);
}

static void
paint_ball(analogtv_input *inp)
{
  analogtv_draw_solid(inp,
                      ANALOGTV_VIS_START + ball.x, ANALOGTV_VIS_START + ball.x + ball.w,
                      ANALOGTV_TOP + ball.y, ANALOGTV_TOP + ball.y + ball.h,
                      ball_ntsc);
}

static void
paint_score(analogtv_input *inp)
{
  char buf[256];

  analogtv_draw_solid(inp,
                      ANALOGTV_VIS_START, ANALOGTV_VIS_END,
                      ANALOGTV_TOP, ANALOGTV_TOP + 10+ score_font.char_h * score_font.y_mult,
                      field_ntsc);

  sprintf(buf, "%d",r_paddle.score%256);
  analogtv_draw_string(inp, &score_font, buf,
                       ANALOGTV_VIS_START + 130, ANALOGTV_TOP + 8,
                       score_ntsc);

  sprintf(buf, "%d",l_paddle.score%256);
  analogtv_draw_string(inp, &score_font, buf,
                       ANALOGTV_VIS_END - 200, ANALOGTV_TOP + 8,
                       score_ntsc);

}

static void
paint_net(analogtv_input *inp)
{
  int x,y;

  x=(ANALOGTV_VIS_START + ANALOGTV_VIS_END)/2;

  for (y=ANALOGTV_TOP; y<ANALOGTV_BOT; y+=6) {
    analogtv_draw_solid(inp, x-2, x+2, y, y+3,
                        net_ntsc);
    analogtv_draw_solid(inp, x-2, x+2, y+3, y+6,
                        field_ntsc);

  }
}

static void
play_pong (void)
{
  erase_ball(inp);

  ball.x += bx;
  ball.y += by;

  if ((random()%40)==0) {
    if (bx>0) bx++; else bx--;
  }

  if (!r_paddle.wait)
  {
    p_logic(&r_paddle);
  }
  if (!l_paddle.wait)
  {
    p_logic(&l_paddle);
  }

  p_hit_top_bottom(&r_paddle);
  p_hit_top_bottom(&l_paddle);

  hit_top_bottom();
  hit_paddle();

  #ifdef OUTPUT_POS
  printf("(%d,%d,%d,%d)\n",ball.x,ball.y,ball.w,ball.h);
  #endif

  paint_score(inp);

  paint_net(inp);

  if (1) {
    paint_paddle(inp, &r_paddle);
    paint_paddle(inp, &l_paddle);
  }
  if (1) paint_ball(inp);

  analogtv_handle_events(tv);

  analogtv_init_signal(tv, 0.04);
  analogtv_reception_update(&reception);
  analogtv_add_signal(tv, &reception);
  analogtv_draw(tv);
}


char *progclass = "pong";

char *defaults [] = {
  ".background: black",
  ".foreground: white",
  "*speed:      6",
  ANALOGTV_DEFAULTS
  "*TVContrast:      150",
  0
};

XrmOptionDescRec options [] = {
  { "-percent",         ".percent",     XrmoptionSepArg, 0 },
  { "-speed",           ".speed",     XrmoptionSepArg, 0 },
  ANALOGTV_OPTIONS
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  init_pong (dpy, window);
  while (1)
    {
      play_pong ();
    }
}

