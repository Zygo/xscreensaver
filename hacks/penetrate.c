/* Copyright (c) 1999 Adam Miller adum@aya.yale.edu
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.

 * penetrate simulates the arcade classic with the cities and the stuff
 * shooting down from the sky and stuff. The computer plays against itself,
 * desperately defending the forces of good against those thingies raining
 * down. Bonus cities are awarded at ever-increasing intervals. Every five
 * levels appears a bonus round. The computer player gets progressively
 * more intelligent as the game progresses. Better aim, more economical with
 * ammo, and better target selection. Points are in the bottom right, and
 * high score is in the bottom left. Start with -smart to have the computer
 * player skip the learning process.

 Version: 0.2
 -- fixed an AI bug that was keeping the computer player a tad weak
 Version: 0.1
 -- first release

 */

#include "screenhack.h"

#define kSleepTime 10000 

#define font_height(font)	  	(font->ascent + font->descent)

#define kCityPause 500000
#define kLevelPause 1
#define SCORE_MISSILE 100
#define kFirstBonus 5000
#define kMinRate 30
#define kMaxRadius 100

typedef struct {
  int alive;
  int x, y;
  int startx, starty;
  int endx, endy;
  int dcity;
  float pos;
  int enemies;
  int jenis;
  int splits;
  XColor color;
} Missile;

typedef struct {
  int alive;
  int x, y, rad, oflaser;
  int max, outgoing;
  XColor color;
} Boom;

typedef struct {
  int alive;
  int x;
  XColor color;
} City;

typedef struct {
  int alive;
  int x, y;
  int startx, starty;
  int endx, endy;
  int oldx, oldy;
  int oldx2, oldy2;
  float velx, vely, fposx, fposy;
  float lenMul;
  XColor color;
  int target;
} Laser;

#define kMaxMissiles 256
#define kMaxBooms 512
#define kMaxLasers 128
#define kBoomRad 40
#define kNumCities 5

#define kLaserLength 12

#define kMissileSpeed 0.003
#define kLaserSpeed (kMissileSpeed * 6)


struct state {
  Display *dpy;
  Window window;

   XFontStruct *font, *scoreFont;
   GC draw_gc, erase_gc, level_gc;
   unsigned int default_fg_pixel;
   XColor scoreColor;

   int bgrowth;
   int lrate, startlrate;
   long loop;
   long score, highscore;
   long nextBonus;
   int numBonus;
   int bround;
   long lastLaser;
   int gamez;
   int aim;
   int econpersen;
   int choosypersen;
   int carefulpersen;
   int smart;
   Colormap cmap;

   Missile missile[kMaxMissiles];
   Boom boom[kMaxBooms];
   City city[kNumCities];
   Laser laser[kMaxLasers];
   int blive[kNumCities];

   int level, levMissiles, levFreq;

   int draw_xlim, draw_ylim;
   int draw_reset;
};


static void Explode(struct state *st, int x, int y, int max, XColor color, int oflaser)
{
  int i;
  Boom *m = 0;
  for (i=0;i<kMaxBooms;i++)
	 if (!st->boom[i].alive) {
		m = &st->boom[i];
		break;
	 }
  if (!m)
	 return;

  m->alive = 1;
  m->x = x;
  m->y = y;
  m->rad = 0;
  if (max > kMaxRadius)
	 max = kMaxRadius;
  m->max = max;
  m->outgoing = 1;
  m->color = color;
  m->oflaser = oflaser;
}

static void launch (struct state *st, int xlim, int ylim, int src)
{
  int i;
  Missile *m = 0, *msrc;
  for (i=0;i<kMaxMissiles;i++)
	 if (!st->missile[i].alive) {
		m = &st->missile[i];
		break;
	 }
  if (!m)
	 return;

  m->alive = 1;
  m->startx = (random() % xlim);
  m->starty = 0;
  m->endy = ylim;
  m->pos = 0.0;
  m->jenis = random() % 360;
  m->splits = 0;
  if (m->jenis < 50) {
    int j = ylim * 0.4;
    if (j) {
	 m->splits = random() % j;
	 if (m->splits < ylim * 0.08)
		m->splits = 0;
    }
  }

  /* special if we're from another missile */
  if (src >= 0) {
	 int dc = random() % (kNumCities - 1);
	 msrc = &st->missile[src];
	 if (dc == msrc->dcity)
		dc++;
	 m->dcity = dc;
	 m->startx = msrc->x;
	 m->starty = msrc->y;
	 if (m->starty > ylim * 0.4 || m->splits <= m->starty)
		m->splits = 0;  /* too far down already */
	 m->jenis = msrc->jenis;
  }
  else
	 m->dcity = random() % kNumCities;
  m->endx = st->city[m->dcity].x + (random() % 20) - 10;
  m->x = m->startx;
  m->y = m->starty;
  m->enemies = 0;

  if (!mono_p) {
	 hsv_to_rgb (m->jenis, 1.0, 1.0,
					 &m->color.red, &m->color.green, &m->color.blue);
	 m->color.flags = DoRed | DoGreen | DoBlue;
	 if (!XAllocColor (st->dpy, st->cmap, &m->color)) {
		m->color.pixel = WhitePixel (st->dpy, DefaultScreen (st->dpy));
		m->color.red = m->color.green = m->color.blue = 0xFFFF;
	 }
  }
}

#define kExpHelp 0.2
#define kSpeedDiff 3.5
#define kMaxToGround 0.75
static int fire(struct state *st, int xlim, int ylim)
{
  int i, j, cnt = 0;
  int dcity;
  long dx, dy, ex, ey;
  Missile *mis = 0;
  Laser *m = 0;
  int untargeted = 0;
  int choosy = 0, economic = 0, careful = 0;
  int suitor[kMaxMissiles];
  int livecity = 0;
  int ytargetmin = ylim * 0.75;
  int deepest = 0;
  int misnum = 0;

  choosy = (random() % 100) < st->choosypersen;
  economic = (random() % 100) < st->econpersen;
  careful = (random() % 100) < st->carefulpersen;

  /* count our cities */
  for (i=0;i<kNumCities;i++)
	 livecity += st->city[i].alive;
  if (livecity == 0)
	 return 1;  /* no guns */

  for (i=0;i<kMaxLasers;i++)
	 if (!st->laser[i].alive) {
		m = &st->laser[i];
		break;
	 }
  if (!m)
	 return 1;

  /* if no missiles on target, no need to be choosy */
  if (choosy) {
	 int choo = 0;
	 for (j=0;j<kMaxMissiles;j++) {
		mis = &st->missile[j];
		if (!mis->alive || (mis->y > ytargetmin))
		  continue;
		if (st->city[mis->dcity].alive)
		  choo++;
	 }
	 if (choo == 0)
		choosy = 0;
  }

  for (j=0;j<kMaxMissiles;j++) {
	 mis = &st->missile[j];
	 suitor[j] = 0;
	 if (!mis->alive || (mis->y > ytargetmin))
		continue;
	 if (choosy && (st->city[mis->dcity].alive == 0))
		continue;
	 ey = mis->starty + ((float) (mis->endy - mis->starty)) * (mis->pos + kExpHelp + (1.0 - mis->pos) / kSpeedDiff);
	 if (ey > ylim * kMaxToGround)
		continue;  /* too far down */
	 cnt++;
	 suitor[j] = 1;
  }

  /* count missiles that are on target and not being targeted */
  if (choosy && economic)
	 for (j=0;j<kMaxMissiles;j++)
		if (suitor[j] && st->missile[j].enemies == 0)
		  untargeted++;

  if (economic)
	 for (j=0;j<kMaxMissiles;j++) {
		if (suitor[j] && cnt > 1)
		  if (st->missile[j].enemies > 0)
			 if (st->missile[j].enemies > 1 || untargeted == 0) {
				suitor[j] = 0;
				cnt--;
			 }
		/* who's closest? biggest threat */
		if (suitor[j] && st->missile[j].y > deepest)
		  deepest = st->missile[j].y;
	 }

  if (deepest > 0 && careful) {
	 /* only target deepest missile */
	 cnt = 1;
	 for (j=0;j<kMaxMissiles;j++)
		if (suitor[j] && st->missile[j].y != deepest)
		  suitor[j] = 0;
  }

  if (cnt == 0)
	 return 1;  /* no targets available */
  cnt = random() % cnt;
  for (j=0;j<kMaxMissiles;j++)
	 if (suitor[j])
		if (cnt-- == 0) {
		  mis = &st->missile[j];
		  misnum = j;
		  break;
		}

  if (mis == 0)
	 return 1;  /* shouldn't happen */

  dcity = random() % livecity;
  for (j=0;j<kNumCities;j++)
	 if (st->city[j].alive)
		if (dcity-- == 0) {
		  dcity = j;
		  break;
		}
  m->startx = st->city[dcity].x;
  m->starty = ylim;
  ex = mis->startx + ((float) (mis->endx - mis->startx)) * (mis->pos + kExpHelp + (1.0 - mis->pos) / kSpeedDiff);
  ey = mis->starty + ((float) (mis->endy - mis->starty)) * (mis->pos + kExpHelp + (1.0 - mis->pos) / kSpeedDiff);
  m->endx = ex + random() % 16 - 8 + (random() % st->aim) - st->aim / 2;
  m->endy = ey + random() % 16 - 8 + (random() % st->aim) - st->aim / 2;
  if (ey > ylim * kMaxToGround)
	 return 0;  /* too far down */
  mis->enemies++;
  m->target = misnum;
  m->x = m->startx;
  m->y = m->starty;
  m->oldx = -1;
  m->oldy = -1;
  m->oldx2 = -1;
  m->oldy2 = -1;
  m->fposx = m->x;
  m->fposy = m->y;
  dx = (m->endx - m->x);
  dy = (m->endy - m->y);
  m->velx = dx / 100.0;
  m->vely = dy / 100.0;
  m->alive = 1;
  /* m->lenMul = (kLaserLength * kLaserLength) / (m->velx * m->velx + m->vely * m->vely); */
  m->lenMul = -(kLaserLength / m->vely);

  if (!mono_p) {
	 m->color.blue = 0x0000;
	 m->color.green = 0xFFFF;
	 m->color.red = 0xFFFF;
	 m->color.flags = DoRed | DoGreen | DoBlue;
	 if (!XAllocColor (st->dpy, st->cmap, &m->color)) {
		m->color.pixel = WhitePixel (st->dpy, DefaultScreen (st->dpy));
		m->color.red = m->color.green = m->color.blue = 0xFFFF;
	 }
  }
  return 1;
}

static void *
penetrate_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int i;
  const char *levelfont = "-*-courier-*-r-*-*-*-380-*-*-*-*-*-*";
  const char *scorefont = "-*-helvetica-*-r-*-*-*-180-*-*-*-*-*-*";
  XGCValues gcv;
  XWindowAttributes xgwa;

  st->dpy = dpy;
  st->window = window;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->cmap = xgwa.colormap;

  st->lrate = 80;
  st->nextBonus = kFirstBonus;
  st->aim = 180;

  st->smart = get_boolean_resource(st->dpy, "smart","Boolean");
  st->bgrowth = get_integer_resource (st->dpy, "bgrowth", "Integer");
  st->lrate = get_integer_resource (st->dpy, "lrate", "Integer");
  if (st->bgrowth < 0) st->bgrowth = 2;
  if (st->lrate < 0) st->lrate = 2;
  st->startlrate = st->lrate;

  st->font = load_font_retry(st->dpy, levelfont);
  if (!st->font) abort();

  st->scoreFont = load_font_retry(st->dpy, scorefont);
  if (!st->scoreFont) abort();

  for (i = 0; i < kMaxMissiles; i++)
    st->missile[i].alive = 0;

  for (i = 0; i < kMaxLasers; i++)
    st->laser[i].alive = 0;

  for (i = 0; i < kMaxBooms; i++)
    st->boom[i].alive = 0;

  for (i = 0; i < kNumCities; i++) {
	 City *m = &st->city[i];
    m->alive = 1;
	 m->color.red = m->color.green = m->color.blue = 0xFFFF;
	 m->color.blue = 0x1111; m->color.green = 0x8888;
	 m->color.flags = DoRed | DoGreen | DoBlue;
	 if (!XAllocColor (st->dpy, st->cmap, &m->color)) {
		m->color.pixel = WhitePixel (st->dpy, DefaultScreen (st->dpy));
		m->color.red = m->color.green = m->color.blue = 0xFFFF;
	 }
  }

  gcv.foreground = st->default_fg_pixel =
    get_pixel_resource(st->dpy, st->cmap, "foreground", "Foreground");
  gcv.font = st->scoreFont->fid;
  st->draw_gc = XCreateGC(st->dpy, st->window, GCForeground | GCFont, &gcv);
  gcv.font = st->font->fid;
  st->level_gc = XCreateGC(st->dpy, st->window, GCForeground | GCFont, &gcv);
  XSetForeground (st->dpy, st->level_gc, st->city[0].color.pixel);
  gcv.foreground = get_pixel_resource(st->dpy, st->cmap, "background", "Background");
  st->erase_gc = XCreateGC(st->dpy, st->window, GCForeground, &gcv);

# ifdef HAVE_JWXYZ
  jwxyz_XSetAntiAliasing (st->dpy, st->erase_gc, False);
  jwxyz_XSetAntiAliasing (st->dpy, st->draw_gc, False);
# endif


  /* make a gray color for score */
  if (!mono_p) {
	 st->scoreColor.red = st->scoreColor.green = st->scoreColor.blue = 0xAAAA;
	 st->scoreColor.flags = DoRed | DoGreen | DoBlue;
	 if (!XAllocColor (st->dpy, st->cmap, &st->scoreColor)) {
		st->scoreColor.pixel = WhitePixel (st->dpy, DefaultScreen (st->dpy));
		st->scoreColor.red = st->scoreColor.green = st->scoreColor.blue = 0xFFFF;
	 }
  }

  XClearWindow(st->dpy, st->window);
  return st;
}

static void DrawScore(struct state *st, int xlim, int ylim)
{
  char buf[16];
  int width, height;
  sprintf(buf, "%ld", st->score);
  width = XTextWidth(st->scoreFont, buf, strlen(buf));
  height = font_height(st->scoreFont);
  XSetForeground (st->dpy, st->draw_gc, st->scoreColor.pixel);
  XFillRectangle(st->dpy, st->window, st->erase_gc,
				  xlim - width - 6, ylim - height - 2, width + 6, height + 2);
  XDrawString(st->dpy, st->window, st->draw_gc, xlim - width - 2, ylim - 2,
		    buf, strlen(buf));

  sprintf(buf, "%ld", st->highscore);
  width = XTextWidth(st->scoreFont, buf, strlen(buf));
  XFillRectangle(st->dpy, st->window, st->erase_gc,
				  4, ylim - height - 2, width + 4, height + 2);
  XDrawString(st->dpy, st->window, st->draw_gc, 4, ylim - 2,
		    buf, strlen(buf));
}

static void AddScore(struct state *st, int xlim, int ylim, long dif)
{
  int i, sumlive = 0;
  for (i=0;i<kNumCities;i++)
	 sumlive += st->city[i].alive;
  if (sumlive == 0)
	 return;   /* no cities, not possible to score */

  st->score += dif;
  if (st->score > st->highscore)
	 st->highscore = st->score;
  DrawScore(st, xlim, ylim);
}

static void DrawCity(struct state *st, int x, int y, XColor col)
{
	 XSetForeground (st->dpy, st->draw_gc, col.pixel);
	 XFillRectangle(st->dpy, st->window, st->draw_gc,
				  x - 30, y - 40, 60, 40);
	 XFillRectangle(st->dpy, st->window, st->draw_gc,
						 x - 20, y - 50, 10, 10);
	 XFillRectangle(st->dpy, st->window, st->draw_gc,
				  x + 10, y - 50, 10, 10);
}

static void DrawCities(struct state *st, int xlim, int ylim)
{
  int i, x;
  for (i = 0; i < kNumCities; i++) {
	 City *m = &st->city[i];
	 if (!m->alive)
		continue;
	 x = (i + 1) * (xlim / (kNumCities + 1));
	 m->x = x;

	 DrawCity(st, x, ylim, m->color);
  }
}

static void LoopMissiles(struct state *st, int xlim, int ylim)
{
  int i, j, max = 0;
  for (i = 0; i < kMaxMissiles; i++) {
	 int old_x, old_y;
	 Missile *m = &st->missile[i];
	 if (!m->alive)
		continue;
	 old_x = m->x;
	 old_y = m->y;
	 m->pos += kMissileSpeed;
	 m->x = m->startx + ((float) (m->endx - m->startx)) * m->pos;
	 m->y = m->starty + ((float) (m->endy - m->starty)) * m->pos;

      /* erase old one */

	 XSetLineAttributes(st->dpy, st->draw_gc, 4, 0,0,0);
    XSetForeground (st->dpy, st->draw_gc, m->color.pixel);
	 XDrawLine(st->dpy, st->window, st->draw_gc,
				  old_x, old_y, m->x, m->y);

	 /* maybe split off a new missile? */
	 if (m->splits && (m->y > m->splits)) {
		m->splits = 0;
		launch(st, xlim, ylim, i);
	 }
	 
	 if (m->y >= ylim) {
		m->alive = 0;
		if (st->city[m->dcity].alive) {
		  st->city[m->dcity].alive = 0;
		  Explode(st, m->x, m->y, kBoomRad * 2, m->color, 0);
		}
	 }

	 /* check hitting explosions */
	 for (j=0;j<kMaxBooms;j++) {
		Boom *b = &st->boom[j];
		if (!b->alive)
		  continue;
		else {
		  int dx = abs(m->x - b->x);
		  int dy = abs(m->y - b->y);
		  int r = b->rad + 2;
		  if ((dx < r) && (dy < r))
			 if (dx * dx + dy * dy < r * r) {
				m->alive = 0;
				max = b->max + st->bgrowth - kBoomRad;
				AddScore(st, xlim, ylim, SCORE_MISSILE);
		  }
		}
	 }

	 if (m->alive == 0) {
		float my_pos;
		/* we just died */
		Explode(st, m->x, m->y, kBoomRad + max, m->color, 0);
		XSetLineAttributes(st->dpy, st->erase_gc, 4, 0,0,0);
		/* In a perfect world, we could simply erase a line from
		   (m->startx, m->starty) to (m->x, m->y). This is not a
		   perfect world. */
		old_x = m->startx;
		old_y = m->starty;
		my_pos = kMissileSpeed;
		while (my_pos <= m->pos) {
			m->x = m->startx + ((float) (m->endx - m->startx)) * my_pos;
			m->y = m->starty + ((float) (m->endy - m->starty)) * my_pos;
			XDrawLine(st->dpy, st->window, st->erase_gc, old_x, old_y, m->x, m->y);
			old_x = m->x;
			old_y = m->y;
			my_pos += kMissileSpeed;
		}
	 }
  }
}

static void LoopLasers(struct state *st, int xlim, int ylim)
{
  int i, j, miny = ylim * 0.8;
  int x, y;
  for (i = 0; i < kMaxLasers; i++) {
	 Laser *m = &st->laser[i];
	 if (!m->alive)
		continue;

	 if (m->oldx != -1) {
		 XSetLineAttributes(st->dpy, st->erase_gc, 2, 0,0,0);
		 XDrawLine(st->dpy, st->window, st->erase_gc,
				  m->oldx2, m->oldy2, m->oldx, m->oldy);
	 }

	 m->fposx += m->velx;
	 m->fposy += m->vely;
	 m->x = m->fposx;
	 m->y = m->fposy;
	 
	 x = m->fposx + (-m->velx * m->lenMul);
	 y = m->fposy + (-m->vely * m->lenMul);

	 m->oldx = x;
	 m->oldy = y;

	 XSetLineAttributes(st->dpy, st->draw_gc, 2, 0,0,0);
    XSetForeground (st->dpy, st->draw_gc, m->color.pixel);
	 XDrawLine(st->dpy, st->window, st->draw_gc,
				  m->x, m->y, x, y);

	 m->oldx2 = m->x;
	 m->oldy2 = m->y;
	 m->oldx = x;
	 m->oldy = y;
	 
	 if (m->y < m->endy) {
		m->alive = 0;
	 }

	 /* check hitting explosions */
	 if (m->y < miny)
		for (j=0;j<kMaxBooms;j++) {
		  Boom *b = &st->boom[j];
		  if (!b->alive)
			 continue;
		  else {
			 int dx = abs(m->x - b->x);
			 int dy = abs(m->y - b->y);
			 int r = b->rad + 2;
			 if (b->oflaser)
				continue;
			 if ((dx < r) && (dy < r))
				if (dx * dx + dy * dy < r * r) {
				  m->alive = 0;
				  /* one less enemy on this missile -- it probably didn't make it */
				  if (st->missile[m->target].alive)
					 st->missile[m->target].enemies--;
				}
		  }
		}
	 
	 if (m->alive == 0) {
		/* we just died */
		XDrawLine(st->dpy, st->window, st->erase_gc,
				  m->x, m->y, x, y);
		Explode(st, m->x, m->y, kBoomRad, m->color, 1);
	 }
  }
}

static void LoopBooms(struct state *st, int xlim, int ylim)
{
  int i;
  for (i = 0; i < kMaxBooms; i++) {
	 Boom *m = &st->boom[i];
	 if (!m->alive)
		continue;
	 
	 if (st->loop & 1) {
		if (m->outgoing) {
		  m->rad++;
		  if (m->rad >= m->max)
			 m->outgoing = 0;
		  XSetLineAttributes(st->dpy, st->draw_gc, 1, 0,0,0);
		  XSetForeground (st->dpy, st->draw_gc, m->color.pixel);
		  XDrawArc(st->dpy, st->window, st->draw_gc, m->x - m->rad, m->y - m->rad, m->rad * 2, m->rad * 2, 0, 360 * 64);
		}
		else {
		  XSetLineAttributes(st->dpy, st->erase_gc, 1, 0,0,0);
		  XDrawArc(st->dpy, st->window, st->erase_gc, m->x - m->rad, m->y - m->rad, m->rad * 2, m->rad * 2, 0, 360 * 64);
		  m->rad--;
		  if (m->rad <= 0)
			 m->alive = 0;
		}
	 }
  }
}


/* after they die, let's change a few things */
static void Improve(struct state *st)
{
  if (st->smart)
	 return;
  if (st->level > 20)
	 return;  /* no need, really */
  st->aim -= 4;
  if (st->level <= 2) st->aim -= 8;
  if (st->level <= 5) st->aim -= 6;
  if (st->gamez < 3)
	 st->aim -= 10;
  st->carefulpersen += 6;
  st->choosypersen += 4;
  if (st->level <= 5) st->choosypersen += 3;
  st->econpersen += 4;
  st->lrate -= 2;
  if (st->startlrate < kMinRate) {
	 if (st->lrate < st->startlrate)
		st->lrate = st->startlrate;
  }
  else {
	 if (st->lrate < kMinRate)
		st->lrate = kMinRate;
  }
  if (st->level <= 5) st->econpersen += 3;
  if (st->aim < 1) st->aim = 1;
  if (st->choosypersen > 100) st->choosypersen = 100;
  if (st->carefulpersen > 100) st->carefulpersen = 100;
  if (st->econpersen > 100) st->econpersen = 100;
}

static void NewLevel(struct state *st, int xlim, int ylim)
{
  char buf[32];
  int width, i, sumlive = 0;
  int liv[kNumCities];
  int freecity = 0;

  if (st->level == 0) {
	 st->level++;
	 goto END_LEVEL;
  }

  /* check for a free city */
  if (st->score >= st->nextBonus) {
	 st->numBonus++;
	 st->nextBonus += kFirstBonus * st->numBonus;
	 freecity = 1;
  }

  for (i=0;i<kNumCities;i++) {
	 if (st->bround)
		st->city[i].alive = st->blive[i];
	 liv[i] = st->city[i].alive;
	 sumlive += liv[i];
	 if (!st->bround)
		st->city[i].alive = 0;
  }

  /* print out screen */
  XFillRectangle(st->dpy, st->window, st->erase_gc,
				  0, 0, xlim, ylim);
  if (st->bround)
	 sprintf(buf, "Bonus Round Over");
  else {
	 if (sumlive || freecity)
		sprintf(buf, "Level %d Cleared", st->level);
	 else
		sprintf(buf, "GAME OVER");
  }
  if (st->level > 0) {
	 width = XTextWidth(st->font, buf, strlen(buf));
	 XDrawString(st->dpy, st->window, st->level_gc, xlim / 2 - width / 2, ylim / 2 - font_height(st->font) / 2,
					 buf, strlen(buf));
	 XSync(st->dpy, False);
	 usleep(1000000);
  }

  if (!st->bround) {
	 if (sumlive || freecity) {
		int sumwidth;
		/* draw live cities */
		XFillRectangle(st->dpy, st->window, st->erase_gc,
							0, ylim - 100, xlim, 100);

		sprintf(buf, "X %ld", st->level * 100L);
		/* how much they get */
		sumwidth = XTextWidth(st->font, buf, strlen(buf));
		/* add width of city */
		sumwidth += 60;
		/* add spacer */
		sumwidth += 40;
		DrawCity(st, xlim / 2 - sumwidth / 2 + 30, ylim * 0.70, st->city[0].color);
		XDrawString(st->dpy, st->window, st->level_gc, xlim / 2 - sumwidth / 2 + 40 + 60, ylim * 0.7, buf, strlen(buf));
		for (i=0;i<kNumCities;i++) {
		  if (liv[i]) {
			 st->city[i].alive = 1;
			 AddScore(st, xlim, ylim, 100 * st->level);
			 DrawCities(st, xlim, ylim);
			 XSync(st->dpy, False);
			 usleep(kCityPause);
		  }
		}
	 }
	 else {
		/* we're dead */
		usleep(3000000);

		/* start new */
		st->gamez++;
		Improve(st);
		for (i=0;i<kNumCities;i++)
		  st->city[i].alive = 1;
		st->level = 0;
		st->loop = 1;
		st->score = 0;
		st->nextBonus = kFirstBonus;
		st->numBonus = 0;
		DrawCities(st, xlim, ylim);
	 }
  }

  /* do free city part */
  if (freecity && sumlive < 5) {
	 int ncnt = random() % (5 - sumlive) + 1;
	 for (i=0;i<kNumCities;i++)
		if (!st->city[i].alive)
		  if (!--ncnt)
			 st->city[i].alive = 1;
	 strcpy(buf, "Bonus City");
	 width = XTextWidth(st->font, buf, strlen(buf));
	 XDrawString(st->dpy, st->window, st->level_gc, xlim / 2 - width / 2, ylim / 4, buf, strlen(buf));
	 DrawCities(st, xlim, ylim);
	 XSync(st->dpy, False);
	 usleep(1000000);
  }

  XFillRectangle(st->dpy, st->window, st->erase_gc,
					  0, 0, xlim, ylim - 100);
  
  if (!st->bround)
	 st->level++;
  if (st->level == 1) {
	 st->nextBonus = kFirstBonus;
  }

  if (st->level > 3 && (st->level % 5 == 1)) {
	 if (st->bround) {
		st->bround = 0;
		DrawCities(st, xlim, ylim);
	 }
	 else {
		/* bonus round */
		st->bround = 1;
		st->levMissiles = 20 + st->level * 10;
		st->levFreq = 10;
		for (i=0;i<kNumCities;i++)
		  st->blive[i] = st->city[i].alive;
		sprintf(buf, "Bonus Round");
		width = XTextWidth(st->font, buf, strlen(buf));
		XDrawString(st->dpy, st->window, st->level_gc, xlim / 2 - width / 2, ylim / 2 - font_height(st->font) / 2, buf, strlen(buf));
		XSync(st->dpy, False);
		usleep(1000000);
		XFillRectangle(st->dpy, st->window, st->erase_gc,
							0, 0, xlim, ylim - 100);
	 }
  }

 END_LEVEL: ;

  if (!st->bround) {
	 st->levMissiles = 5 + st->level * 3;
	 if (st->level > 5)
		st->levMissiles += st->level * 5;
	 /*  levMissiles = 2; */
	 st->levFreq = 120 - st->level * 5;
	 if (st->levFreq < 30)
		st->levFreq = 30;
  }

  /* ready to fire */
  st->lastLaser = 0;
}


static unsigned long
penetrate_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XWindowAttributes xgwa;

  if (st->draw_reset)
    {
      st->draw_reset = 0;
      DrawCities(st, st->draw_xlim, st->draw_ylim);
    }

  XGetWindowAttributes(st->dpy, st->window, &xgwa);
  st->draw_xlim = xgwa.width;
  st->draw_ylim = xgwa.height;

  /* see if just started */
  if (st->loop == 0) {
	 if (st->smart) {
		st->choosypersen = st->econpersen = st->carefulpersen = 100;
		st->lrate = kMinRate; st->aim = 1;
	 }
	 NewLevel(st, st->draw_xlim, st->draw_ylim);
	 DrawScore(st, st->draw_xlim, st->draw_ylim);
  }

  st->loop++;

  if (st->levMissiles == 0) {
	 /* see if anything's still on the screen, to know when to end level */
	 int i;
	 for (i=0;i<kMaxMissiles;i++)
		if (st->missile[i].alive)
		  goto END_CHECK;
	 for (i=0;i<kMaxBooms;i++)
		if (st->boom[i].alive)
		  goto END_CHECK;
	 for (i=0;i<kMaxLasers;i++)
		if (st->laser[i].alive)
		  goto END_CHECK;
	 /* okay, nothing's alive, start end of level countdown */
	 usleep(kLevelPause*1000000);
	 NewLevel(st, st->draw_xlim, st->draw_ylim);
         goto END;
  END_CHECK: ;
  }
  else if ((random() % st->levFreq) == 0) {
	 launch(st, st->draw_xlim, st->draw_ylim, -1);
	 st->levMissiles--;
  }

  if (st->loop - st->lastLaser >= st->lrate) {
	 if (fire(st, st->draw_xlim, st->draw_ylim))
		st->lastLaser = st->loop;
  }

  if ((st->loop & 7) == 0)
    st->draw_reset = 1;

  LoopMissiles(st, st->draw_xlim, st->draw_ylim);
  LoopLasers(st, st->draw_xlim, st->draw_ylim);
  LoopBooms(st, st->draw_xlim, st->draw_ylim);

 END:
  return kSleepTime;
}

static void
penetrate_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  XClearWindow (dpy, window);
}

static Bool
penetrate_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
penetrate_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *penetrate_defaults [] = {
  ".lowrez:     true",
  ".background:	black",
  ".foreground:	white",
  "*fpsTop:	true",
  "*fpsSolid:	true",
  "*bgrowth:	5",
  "*lrate:	80",
  "*smart:	False",
  0
};

static XrmOptionDescRec penetrate_options [] = {
  { "-bgrowth",		".bgrowth",	XrmoptionSepArg, 0 },
  { "-lrate",		".lrate",	XrmoptionSepArg, 0 },
  {"-smart",            ".smart",       XrmoptionNoArg, "True" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Penetrate", penetrate)
