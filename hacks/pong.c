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
 * Modified by Dave Odell <dmo2118@gmail.com> to add -p1 and -p2 options.
 * JWXYZ doesn't support XWarpPointer, so PLAYER_MOUSE only works on native
 * X11. JWXYZ also doesn't support cursors, so PLAYER_TABLET doesn't hide the
 * mouse pointer.
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
#include <time.h>
#ifndef HAVE_JWXYZ
# include <X11/keysym.h>
#endif
/* #define OUTPUT_POS */

typedef enum {
  PLAYER_AI,
  PLAYER_MOUSE,
  PLAYER_TABLET,
  PLAYER_KEYBOARD,
  PLAYER_KEYBOARD_LEFT
} PlayerType;

typedef struct _paddle {
  PlayerType player;
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

# ifndef HAVE_JWXYZ
  Cursor null_cursor;
# endif
  int mouse_y;
  unsigned w, h, screen_h, screen_h_mm;
  Bool is_focused;
  Bool key_w: 1;
  Bool key_s: 1;
  Bool key_up: 1;
  Bool key_down: 1;
  unsigned int dragging : 2;
};


enum {
  PONG_W = ANALOGTV_VIS_LEN,
  PONG_H = ANALOGTV_VISLINES,
  PONG_TMARG = 10
};

static void
p_hit_top_bottom(Paddle *p);

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
  /* Adjust paddle position again, because
     paddle length is enlarged (reset) above. */
  p_hit_top_bottom(&st->l_paddle);
  p_hit_top_bottom(&st->r_paddle);
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

static PlayerType
get_player_type(Display *dpy, char *rsrc)
{
  PlayerType result;
  char *s = get_string_resource(dpy, rsrc, "String");
  if (!strcmp(s, "ai") || !strcmp(s, "AI"))
  {
    result = PLAYER_AI;
  }
# ifndef HAVE_JWXYZ
  else if (!strcmp(s, "mouse"))
  {
    result = PLAYER_MOUSE;
  }
# endif
  else if (!strcmp(s, "tab") || !strcmp(s, "tablet"))
  {
    result = PLAYER_TABLET;
  }
  else if (!strcmp(s, "kb") || !strcmp(s, "keyb") || !strcmp(s, "keyboard") ||
           !strcmp(s, "right") || !strcmp(s, "kbright") ||
           !strcmp(s, "arrows"))
  {
    result = PLAYER_KEYBOARD;
  }
  else if (!strcmp(s, "left") || !strcmp(s, "kbleft") ||
           !strcmp(s, "ws") || !strcmp(s, "wasd"))
  {
    result = PLAYER_KEYBOARD_LEFT;
  }
  else
  {
    fprintf(stderr, "%s: invalid player type\n", progname);
    result = PLAYER_AI;
  }
  free(s);
  return result;
}

static void
do_shape (struct state *st, const XWindowAttributes *xgwa)
{
  st->w = xgwa->width;
  st->h = xgwa->height;
  st->screen_h = XHeightOfScreen(xgwa->screen);
  st->screen_h_mm = XHeightMMOfScreen(xgwa->screen);
}

#ifndef HAVE_JWXYZ
static Bool
needs_grab (struct state *st)
{
  return
  st->l_paddle.player == PLAYER_MOUSE ||
  st->r_paddle.player == PLAYER_MOUSE;
/*
  st->l_paddle.player == PLAYER_TABLET ||
  st->r_paddle.player == PLAYER_TABLET;
 */
}

static void
grab_pointer (struct state *st)
{
  st->is_focused = True;
  XGrabPointer(st->dpy, st->window, True, PointerMotionMask, GrabModeAsync,
               GrabModeAsync, st->window, st->null_cursor, CurrentTime);
}
#endif /* !HAVE_JWXYZ */

static void *
pong_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));

  int i;
  XWindowAttributes xgwa;
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
  st->l_paddle.player = get_player_type(dpy, "p1");
  st->l_paddle.x = 8;
  st->l_paddle.y = 100;
  st->l_paddle.w = 16;
  st->l_paddle.h = PONG_H/4;
  st->l_paddle.wait = 1;
  st->l_paddle.lock = 0;
  st->r_paddle = st->l_paddle;
  st->r_paddle.player = get_player_type(dpy, "p2");
  st->r_paddle.x = PONG_W - 8 - st->r_paddle.w;
  st->r_paddle.wait = 0;
  /*Init the ball*/
  st->ball.x = PONG_W/2;
  st->ball.y = PONG_H/2;
  st->ball.w = 16;
  st->ball.h = 8;

  /* The mouse warping business breaks tablet input. */
  if (st->l_paddle.player == PLAYER_MOUSE &&
      st->r_paddle.player == PLAYER_TABLET)
  {
    st->l_paddle.player = PLAYER_TABLET;
  }
  if (st->r_paddle.player == PLAYER_MOUSE &&
      st->l_paddle.player == PLAYER_TABLET)
  {
    st->r_paddle.player = PLAYER_TABLET;
  }

  if (st->clock) {
    st->l_paddle.player = PLAYER_AI;
    st->r_paddle.player = PLAYER_AI;
    fprintf(stderr, "%s: clock mode requires AI control\n", progname);

  }

# ifndef HAVE_JWXYZ
  if (st->l_paddle.player == PLAYER_MOUSE ||
      st->r_paddle.player == PLAYER_MOUSE ||
      st->l_paddle.player == PLAYER_TABLET ||
      st->r_paddle.player == PLAYER_TABLET)
  {
    XColor black = {0, 0, 0, 0};
    Pixmap cursor_pix = XCreatePixmap(dpy, window, 4, 4, 1);
    XGCValues gcv;
    GC mono_gc;

    gcv.foreground = 0;
    mono_gc = XCreateGC(dpy, cursor_pix, GCForeground, &gcv);
    st->null_cursor = XCreatePixmapCursor(dpy, cursor_pix, cursor_pix,
                                          &black, &black, 0, 0);
    XFillRectangle(dpy, cursor_pix, mono_gc, 0, 0, 4, 4);
    XFreeGC(dpy, mono_gc);

    XSelectInput(dpy, window,
                 PointerMotionMask | FocusChangeMask |
                 KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask);

    if (needs_grab(st))
    {
      grab_pointer(st);
    }
    else
    {
      XDefineCursor(dpy, window, st->null_cursor);
    }
  }
# endif

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

  XGetWindowAttributes(dpy, window, &xgwa);
  do_shape(st, &xgwa);

  return st;
}

static void
p_logic(struct state *st, Paddle *p)
{
  if (p->player == PLAYER_AI)
  {
    if (!p->wait)
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
  }
# ifndef HAVE_JWXYZ
  else if (p->player == PLAYER_MOUSE)
  {
    /* Clipping happens elsewhere. */
    /* As the screen resolution increases, the mouse moves faster in terms of
       pixels, so divide by DPI. */
    p->y += (int)(st->mouse_y - (st->h / 2)) * 4 * (int)st->screen_h_mm / (3 * (int)st->screen_h);
    if (st->is_focused)
      XWarpPointer (st->dpy, None, st->window, 0, 0, 0, 0, st->w / 2, st->h / 2);
  }
# endif
  else if (p->player == PLAYER_TABLET)
  {
    p->y = st->mouse_y * (PONG_H - PONG_TMARG) / st->h + PONG_TMARG - p->h / 2;
  }
  else if (p->player == PLAYER_KEYBOARD)
  {
    if (st->key_up)
      p->y -= 8;
    if (st->key_down)
      p->y += 8;
  }
  else if (p->player == PLAYER_KEYBOARD_LEFT)
  {
    if (st->key_w)
      p->y -= 8;
    if (st->key_s)
      p->y += 8;
  }

  if ((st->dragging == 1 && p == &st->l_paddle) ||
      (st->dragging == 2 && p == &st->r_paddle))
    {
      /* Not getting MotionNotify. */
      Window root1, child1;
      int mouse_x, mouse_y, root_x, root_y;
      unsigned int mask;
      if (XQueryPointer (st->dpy, st->window, &root1, &child1,
                         &root_x, &root_y, &mouse_x, &mouse_y, &mask))
        st->mouse_y = mouse_y;

      if (st->mouse_y < 0) st->mouse_y = 0;
      p->y = st->mouse_y * (PONG_H - PONG_TMARG) / st->h + PONG_TMARG - p->h / 2;
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

static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}

static unsigned long
pong_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  const analogtv_reception *reception = &st->reception;
  double then = double_time(), now, timedelta;

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

  p_logic(st, &st->r_paddle);
  p_logic(st, &st->l_paddle);

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

  analogtv_reception_update(&st->reception);
  analogtv_draw(st->tv, st->noise, &reception, 1);

  now = double_time();
  timedelta = (1 / 29.97) - (now - then);
  return timedelta > 0 ? timedelta * 1000000 : 0;
}



static const char *pong_defaults [] = {
  ".background: black",
  ".foreground: white",
  "*speed:      6",
  "*noise:      0.04",
  "*clock:      false",
  "*p1:         ai",
  "*p2:         ai",
  ANALOGTV_DEFAULTS
  0
};

static XrmOptionDescRec pong_options [] = {
  { "-speed",           ".speed",     XrmoptionSepArg, 0 },
  { "-noise",           ".noise",     XrmoptionSepArg, 0 },
  { "-clock",           ".clock",     XrmoptionNoArg, "true" },
  { "-p1",              ".p1",        XrmoptionSepArg, 0 },
  { "-p2",              ".p2",        XrmoptionSepArg, 0 },
  ANALOGTV_OPTIONS
  { 0, 0, 0, 0 }
};

static void
pong_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XWindowAttributes xgwa;
  analogtv_reconfigure (st->tv);

  XGetWindowAttributes(dpy, window, &xgwa); /* AnalogTV does this too. */
  xgwa.width = w;
  xgwa.height = h;
  do_shape(st, &xgwa);
}

static Bool
pong_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  switch (event->type)
  {
  case MotionNotify:
    st->mouse_y = event->xmotion.y;
    break;
# ifndef HAVE_JWXYZ
  case FocusIn:
    if (needs_grab(st))
    {
      grab_pointer(st);
    }
    break;
  case FocusOut:
    if (needs_grab(st))
    {
      XUngrabPointer (dpy, CurrentTime);
      st->is_focused = False;
    }
    break;
# endif /* !HAVE_JWXYZ */
  case KeyPress:
  case KeyRelease:
    {
      char c;
      KeySym key;
      XLookupString(&event->xkey, &c, 1, &key, 0);
      Bool is_pressed = event->type == KeyPress;
      switch(key)
      {
      case XK_Up:
        if (st->l_paddle.player == PLAYER_KEYBOARD ||
            st->r_paddle.player == PLAYER_KEYBOARD)
          {
            st->key_up = is_pressed;
            return True;
          }
        break;
      case XK_Down:
        if (st->l_paddle.player == PLAYER_KEYBOARD ||
            st->r_paddle.player == PLAYER_KEYBOARD)
          {
            st->key_down = is_pressed;
            return True;
          }
        break;
      case 'w':
        if (st->l_paddle.player == PLAYER_KEYBOARD_LEFT ||
            st->r_paddle.player == PLAYER_KEYBOARD_LEFT)
          {
            st->key_w = is_pressed;
            return True;
          }
        break;
      case 's':
        if (st->l_paddle.player == PLAYER_KEYBOARD_LEFT ||
            st->r_paddle.player == PLAYER_KEYBOARD_LEFT)
          {
            st->key_s = is_pressed;
            return True;
          }
        break;
      }
    }

  /* Allow the user to pick up and drag either paddle with the mouse,
     even when not in a mouse-paddle mode. */

  case ButtonPress:
    if (st->dragging != 0)
      return False;
    else if (event->xbutton.x < st->w * 0.2)
      {
        if (st->l_paddle.player != PLAYER_MOUSE)
          st->dragging = 1;
        return True;
      }
    else if (event->xbutton.x > st->w * 0.8)
      {
        if (st->r_paddle.player != PLAYER_MOUSE)
          st->dragging = 2;
        return True;
      }
    break;
  case ButtonRelease:
    if (st->dragging != 0)
      {
        st->dragging = 0;
        return True;
      }
    break;
  }
  return False;
}

static void
pong_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  analogtv_release(st->tv);
  free(st->inp);
  if (st->score_font.text_im) XDestroyImage (st->score_font.text_im);
  free (st);
}

XSCREENSAVER_MODULE ("Pong", pong)
