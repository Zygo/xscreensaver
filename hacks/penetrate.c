/* Copyright (c) 1999
 *  Adam Miller adum@aya.yale.edu
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
#define FONT_NAME			"-*-times-*-*-*-*-80-*-*-*-*-*-*-*"

#define kCityPause 500000
#define kLevelPause 1
#define SCORE_MISSILE 100
#define kFirstBonus 5000
#define kMinRate 30
#define kMaxRadius 100

static XFontStruct *font, *scoreFont;
static GC draw_gc, erase_gc, level_gc;
static unsigned int default_fg_pixel;
static XColor scoreColor;

int bgrowth;
int lrate = 80, startlrate;
long loop = 0;
long score = 0, highscore = 0;
long nextBonus = kFirstBonus;
int numBonus = 0;
int bround = 0;
long lastLaser = 0;
int gamez = 0;
int aim = 180;
int econpersen = 0;
int choosypersen = 0;
int carefulpersen = 0;
int smart = 0;

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

Missile missile[kMaxMissiles];
Boom boom[kMaxBooms];
City city[kNumCities];
Laser laser[kMaxLasers];
int blive[kNumCities];

static void Explode(int x, int y, int max, XColor color, int oflaser)
{
  int i;
  Boom *m = 0;
  for (i=0;i<kMaxBooms;i++)
	 if (!boom[i].alive) {
		m = &boom[i];
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

static void launch (int xlim, int ylim,
	Display *dpy, Colormap cmap, int src)
{
  int i;
  Missile *m = 0, *msrc;
  for (i=0;i<kMaxMissiles;i++)
	 if (!missile[i].alive) {
		m = &missile[i];
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
	 m->splits = random() % ((int) (ylim * 0.4));
	 if (m->splits < ylim * 0.08)
		m->splits = 0;
  }

  /* special if we're from another missile */
  if (src >= 0) {
	 int dc = random() % (kNumCities - 1);
	 msrc = &missile[src];
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
  m->endx = city[m->dcity].x + (random() % 20) - 10;
  m->x = m->startx;
  m->y = m->starty;
  m->enemies = 0;

  if (!mono_p) {
	 hsv_to_rgb (m->jenis, 1.0, 1.0,
					 &m->color.red, &m->color.green, &m->color.blue);
	 m->color.flags = DoRed | DoGreen | DoBlue;
	 if (!XAllocColor (dpy, cmap, &m->color)) {
		m->color.pixel = WhitePixel (dpy, DefaultScreen (dpy));
		m->color.red = m->color.green = m->color.blue = 0xFFFF;
	 }
  }
}

#define kExpHelp 0.2
#define kSpeedDiff 3.5
#define kMaxToGround 0.75
static int fire(int xlim, int ylim,
	Display *dpy, Window window, Colormap cmap)
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

  choosy = (random() % 100) < choosypersen;
  economic = (random() % 100) < econpersen;
  careful = (random() % 100) < carefulpersen;

  /* count our cities */
  for (i=0;i<kNumCities;i++)
	 livecity += city[i].alive;
  if (livecity == 0)
	 return 1;  /* no guns */

  for (i=0;i<kMaxLasers;i++)
	 if (!laser[i].alive) {
		m = &laser[i];
		break;
	 }
  if (!m)
	 return 1;

  /* if no missiles on target, no need to be choosy */
  if (choosy) {
	 int choo = 0;
	 for (j=0;j<kMaxMissiles;j++) {
		mis = &missile[j];
		if (!mis->alive || (mis->y > ytargetmin))
		  continue;
		if (city[mis->dcity].alive)
		  choo++;
	 }
	 if (choo == 0)
		choosy = 0;
  }

  for (j=0;j<kMaxMissiles;j++) {
	 mis = &missile[j];
	 suitor[j] = 0;
	 if (!mis->alive || (mis->y > ytargetmin))
		continue;
	 if (choosy && (city[mis->dcity].alive == 0))
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
		if (suitor[j] && missile[j].enemies == 0)
		  untargeted++;

  if (economic)
	 for (j=0;j<kMaxMissiles;j++) {
		if (suitor[j] && cnt > 1)
		  if (missile[j].enemies > 0)
			 if (missile[j].enemies > 1 || untargeted == 0) {
				suitor[j] = 0;
				cnt--;
			 }
		/* who's closest? biggest threat */
		if (suitor[j] && missile[j].y > deepest)
		  deepest = missile[j].y;
	 }

  if (deepest > 0 && careful) {
	 /* only target deepest missile */
	 cnt = 1;
	 for (j=0;j<kMaxMissiles;j++)
		if (suitor[j] && missile[j].y != deepest)
		  suitor[j] = 0;
  }

  if (cnt == 0)
	 return 1;  /* no targets available */
  cnt = random() % cnt;
  for (j=0;j<kMaxMissiles;j++)
	 if (suitor[j])
		if (cnt-- == 0) {
		  mis = &missile[j];
		  misnum = j;
		  break;
		}

  if (mis == 0)
	 return 1;  /* shouldn't happen */

  dcity = random() % livecity;
  for (j=0;j<kNumCities;j++)
	 if (city[j].alive)
		if (dcity-- == 0) {
		  dcity = j;
		  break;
		}
  m->startx = city[dcity].x;
  m->starty = ylim;
  ex = mis->startx + ((float) (mis->endx - mis->startx)) * (mis->pos + kExpHelp + (1.0 - mis->pos) / kSpeedDiff);
  ey = mis->starty + ((float) (mis->endy - mis->starty)) * (mis->pos + kExpHelp + (1.0 - mis->pos) / kSpeedDiff);
  m->endx = ex + random() % 16 - 8 + (random() % aim) - aim / 2;
  m->endy = ey + random() % 16 - 8 + (random() % aim) - aim / 2;
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
	 if (!XAllocColor (dpy, cmap, &m->color)) {
		m->color.pixel = WhitePixel (dpy, DefaultScreen (dpy));
		m->color.red = m->color.green = m->color.blue = 0xFFFF;
	 }
  }
  return 1;
}

static Colormap
init_penetrate(Display *dpy, Window window)
{
  int i;
  /*char *fontname =   "-*-new century schoolbook-*-r-*-*-*-380-*-*-*-*-*-*"; */
  char *fontname =   "-*-courier-*-r-*-*-*-380-*-*-*-*-*-*";
  char **list;
  int foo;
  Colormap cmap;
  XGCValues gcv;
  XWindowAttributes xgwa;
  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;

  if (get_string_resource("smart","String")!=NULL && get_string_resource("smart","String")[0]!=0)
	 smart = 1;
  bgrowth = get_integer_resource ("bgrowth", "Integer");
  lrate = get_integer_resource ("lrate", "Integer");
  if (bgrowth < 0) bgrowth = 2;
  if (lrate < 0) lrate = 2;
  startlrate = lrate;

  if (!fontname || !(font = XLoadQueryFont(dpy, fontname))) {
	 list = XListFonts(dpy, FONT_NAME, 32767, &foo);
	 for (i = 0; i < foo; i++)
		if ((font = XLoadQueryFont(dpy, list[i])))
		  break;
	 if (!font) {
		fprintf (stderr, "%s: Can't find a large font.", progname);
	    exit (1);
	 }
	 XFreeFontNames(list);
  }

  if (!(scoreFont = XLoadQueryFont(dpy, "-*-times-*-r-*-*-*-180-*-*-*-*-*-*")))
	 fprintf(stderr, "%s: Can't load Times font.", progname);

  for (i = 0; i < kMaxMissiles; i++)
    missile[i].alive = 0;

  for (i = 0; i < kMaxLasers; i++)
    laser[i].alive = 0;

  for (i = 0; i < kMaxBooms; i++)
    boom[i].alive = 0;

  for (i = 0; i < kNumCities; i++) {
	 City *m = &city[i];
    m->alive = 1;
	 m->color.red = m->color.green = m->color.blue = 0xFFFF;
	 m->color.blue = 0x1111; m->color.green = 0x8888;
	 m->color.flags = DoRed | DoGreen | DoBlue;
	 if (!XAllocColor (dpy, cmap, &m->color)) {
		m->color.pixel = WhitePixel (dpy, DefaultScreen (dpy));
		m->color.red = m->color.green = m->color.blue = 0xFFFF;
	 }
  }

  gcv.foreground = default_fg_pixel =
    get_pixel_resource("foreground", "Foreground", dpy, cmap);
  gcv.font = scoreFont->fid;
  draw_gc = XCreateGC(dpy, window, GCForeground | GCFont, &gcv);
  gcv.font = font->fid;
  level_gc = XCreateGC(dpy, window, GCForeground | GCFont, &gcv);
  XSetForeground (dpy, level_gc, city[0].color.pixel);
  gcv.foreground = get_pixel_resource("background", "Background", dpy, cmap);
  erase_gc = XCreateGC(dpy, window, GCForeground, &gcv);

  /* make a gray color for score */
  if (!mono_p) {
	 scoreColor.red = scoreColor.green = scoreColor.blue = 0xAAAA;
	 scoreColor.flags = DoRed | DoGreen | DoBlue;
	 if (!XAllocColor (dpy, cmap, &scoreColor)) {
		scoreColor.pixel = WhitePixel (dpy, DefaultScreen (dpy));
		scoreColor.red = scoreColor.green = scoreColor.blue = 0xFFFF;
	 }
  }

  XClearWindow(dpy, window);
  return cmap;
}

static void DrawScore(Display *dpy, Window window, Colormap cmap, int xlim, int ylim)
{
  char buf[16];
  int width, height;
  sprintf(buf, "%ld", score);
  width = XTextWidth(scoreFont, buf, strlen(buf));
  height = font_height(scoreFont);
  XSetForeground (dpy, draw_gc, scoreColor.pixel);
  XFillRectangle(dpy, window, erase_gc,
				  xlim - width - 6, ylim - height - 2, width + 6, height + 2);
  XDrawString(dpy, window, draw_gc, xlim - width - 2, ylim - 2,
		    buf, strlen(buf));

  sprintf(buf, "%ld", highscore);
  width = XTextWidth(scoreFont, buf, strlen(buf));
  XFillRectangle(dpy, window, erase_gc,
				  4, ylim - height - 2, width + 4, height + 2);
  XDrawString(dpy, window, draw_gc, 4, ylim - 2,
		    buf, strlen(buf));
}

static void AddScore(Display *dpy, Window window, Colormap cmap, int xlim, int ylim, long dif)
{
  int i, sumlive = 0;
  for (i=0;i<kNumCities;i++)
	 sumlive += city[i].alive;
  if (sumlive == 0)
	 return;   /* no cities, not possible to score */

  score += dif;
  if (score > highscore)
	 highscore = score;
  DrawScore(dpy, window, cmap, xlim, ylim);
}

static void DrawCity(Display *dpy, Window window, Colormap cmap, int x, int y, XColor col)
{
	 XSetForeground (dpy, draw_gc, col.pixel);
	 XFillRectangle(dpy, window, draw_gc,
				  x - 30, y - 40, 60, 40);
	 XFillRectangle(dpy, window, draw_gc,
						 x - 20, y - 50, 10, 10);
	 XFillRectangle(dpy, window, draw_gc,
				  x + 10, y - 50, 10, 10);
}

static void DrawCities(Display *dpy, Window window, Colormap cmap, int xlim, int ylim)
{
  int i, x;
  for (i = 0; i < kNumCities; i++) {
	 City *m = &city[i];
	 if (!m->alive)
		continue;
	 x = (i + 1) * (xlim / (kNumCities + 1));
	 m->x = x;

	 DrawCity(dpy, window, cmap, x, ylim, m->color);
  }
}

static void LoopMissiles(Display *dpy, Window window, Colormap cmap, int xlim, int ylim)
{
  int i, j, max = 0;
  for (i = 0; i < kMaxMissiles; i++) {
	 int old_x, old_y;
	 Missile *m = &missile[i];
	 if (!m->alive)
		continue;
	 old_x = m->x;
	 old_y = m->y;
	 m->pos += kMissileSpeed;
	 m->x = m->startx + ((float) (m->endx - m->startx)) * m->pos;
	 m->y = m->starty + ((float) (m->endy - m->starty)) * m->pos;

      /* erase old one */

	 XSetLineAttributes(dpy, draw_gc, 4, 0,0,0);
    XSetForeground (dpy, draw_gc, m->color.pixel);
	 XDrawLine(dpy, window, draw_gc,
				  old_x, old_y, m->x, m->y);

	 /* maybe split off a new missile? */
	 if (m->splits && (m->y > m->splits)) {
		m->splits = 0;
		launch(xlim, ylim, dpy, cmap, i);
	 }
	 
	 if (m->y >= ylim) {
		m->alive = 0;
		if (city[m->dcity].alive) {
		  city[m->dcity].alive = 0;
		  Explode(m->x, m->y, kBoomRad * 2, m->color, 0);
		}
	 }

	 /* check hitting explosions */
	 for (j=0;j<kMaxBooms;j++) {
		Boom *b = &boom[j];
		if (!b->alive)
		  continue;
		else {
		  int dx = abs(m->x - b->x);
		  int dy = abs(m->y - b->y);
		  int r = b->rad + 2;
		  if ((dx < r) && (dy < r))
			 if (dx * dx + dy * dy < r * r) {
				m->alive = 0;
				max = b->max + bgrowth - kBoomRad;
				AddScore(dpy, window, cmap, xlim, ylim, SCORE_MISSILE);
		  }
		}
	 }

	 if (m->alive == 0) {
		int old_x, old_y;
		float my_pos;
		/* we just died */
		Explode(m->x, m->y, kBoomRad + max, m->color, 0);
		XSetLineAttributes(dpy, erase_gc, 4, 0,0,0);
		/* In a perfect world, we could simply erase a line from
		   (m->startx, m->starty) to (m->x, m->y). This is not a
		   perfect world. */
		old_x = m->startx;
		old_y = m->starty;
		my_pos = kMissileSpeed;
		while (my_pos <= m->pos) {
			m->x = m->startx + ((float) (m->endx - m->startx)) * my_pos;
			m->y = m->starty + ((float) (m->endy - m->starty)) * my_pos;
			XDrawLine(dpy, window, erase_gc, old_x, old_y, m->x, m->y);
			old_x = m->x;
			old_y = m->y;
			my_pos += kMissileSpeed;
		}
	 }
  }
}

static void LoopLasers(Display *dpy, Window window, Colormap cmap, int xlim, int ylim)
{
  int i, j, miny = ylim * 0.8;
  int x, y;
  for (i = 0; i < kMaxLasers; i++) {
	 Laser *m = &laser[i];
	 if (!m->alive)
		continue;

	 if (m->oldx != -1) {
		 XSetLineAttributes(dpy, erase_gc, 2, 0,0,0);
		 XDrawLine(dpy, window, erase_gc,
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

	 XSetLineAttributes(dpy, draw_gc, 2, 0,0,0);
    XSetForeground (dpy, draw_gc, m->color.pixel);
	 XDrawLine(dpy, window, draw_gc,
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
		  Boom *b = &boom[j];
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
				  if (missile[m->target].alive)
					 missile[m->target].enemies--;
				}
		  }
		}
	 
	 if (m->alive == 0) {
		/* we just died */
		XDrawLine(dpy, window, erase_gc,
				  m->x, m->y, x, y);
		Explode(m->x, m->y, kBoomRad, m->color, 1);
	 }
  }
}

static void LoopBooms(Display *dpy, Window window, Colormap cmap, int xlim, int ylim)
{
  int i;
  for (i = 0; i < kMaxBooms; i++) {
	 Boom *m = &boom[i];
	 if (!m->alive)
		continue;
	 
	 if (loop & 1) {
		if (m->outgoing) {
		  m->rad++;
		  if (m->rad >= m->max)
			 m->outgoing = 0;
		  XSetLineAttributes(dpy, draw_gc, 1, 0,0,0);
		  XSetForeground (dpy, draw_gc, m->color.pixel);
		  XDrawArc(dpy, window, draw_gc, m->x - m->rad, m->y - m->rad, m->rad * 2, m->rad * 2, 0, 360 * 64);
		}
		else {
		  XSetLineAttributes(dpy, erase_gc, 1, 0,0,0);
		  XDrawArc(dpy, window, erase_gc, m->x - m->rad, m->y - m->rad, m->rad * 2, m->rad * 2, 0, 360 * 64);
		  m->rad--;
		  if (m->rad <= 0)
			 m->alive = 0;
		}
	 }
  }
}

int level = 0, levMissiles, levFreq;

/* after they die, let's change a few things */
static void Improve(void)
{
  if (smart)
	 return;
  if (level > 20)
	 return;  /* no need, really */
  aim -= 4;
  if (level <= 2) aim -= 8;
  if (level <= 5) aim -= 6;
  if (gamez < 3)
	 aim -= 10;
  carefulpersen += 6;
  choosypersen += 4;
  if (level <= 5) choosypersen += 3;
  econpersen += 4;
  lrate -= 2;
  if (startlrate < kMinRate) {
	 if (lrate < startlrate)
		lrate = startlrate;
  }
  else {
	 if (lrate < kMinRate)
		lrate = kMinRate;
  }
  if (level <= 5) econpersen += 3;
  if (aim < 1) aim = 1;
  if (choosypersen > 100) choosypersen = 100;
  if (carefulpersen > 100) carefulpersen = 100;
  if (econpersen > 100) econpersen = 100;
}

static void NewLevel(Display *dpy, Window window, Colormap cmap, int xlim, int ylim)
{
  char buf[32];
  int width, i, sumlive = 0;
  int liv[kNumCities];
  int freecity = 0;

  if (level == 0) {
	 level++;
	 goto END_LEVEL;
  }

  /* check for a free city */
  if (score >= nextBonus) {
	 numBonus++;
	 nextBonus += kFirstBonus * numBonus;
	 freecity = 1;
  }

  for (i=0;i<kNumCities;i++) {
	 if (bround)
		city[i].alive = blive[i];
	 liv[i] = city[i].alive;
	 sumlive += liv[i];
	 if (!bround)
		city[i].alive = 0;
  }

  /* print out screen */
  XFillRectangle(dpy, window, erase_gc,
				  0, 0, xlim, ylim);
  if (bround)
	 sprintf(buf, "Bonus Round Over");
  else {
	 if (sumlive || freecity)
		sprintf(buf, "Level %d Cleared", level);
	 else
		sprintf(buf, "GAME OVER");
  }
  if (level > 0) {
	 width = XTextWidth(font, buf, strlen(buf));
	 XDrawString(dpy, window, level_gc, xlim / 2 - width / 2, ylim / 2 - font_height(font) / 2,
					 buf, strlen(buf));
	 XSync(dpy, False);
         screenhack_handle_events(dpy);
	 sleep(1);
  }

  if (!bround) {
	 if (sumlive || freecity) {
		int sumwidth;
		/* draw live cities */
		XFillRectangle(dpy, window, erase_gc,
							0, ylim - 100, xlim, 100);

		sprintf(buf, "X %ld", level * 100L);
		/* how much they get */
		sumwidth = XTextWidth(font, buf, strlen(buf));
		/* add width of city */
		sumwidth += 60;
		/* add spacer */
		sumwidth += 40;
		DrawCity(dpy, window, cmap, xlim / 2 - sumwidth / 2 + 30, ylim * 0.70, city[0].color);
		XDrawString(dpy, window, level_gc, xlim / 2 - sumwidth / 2 + 40 + 60, ylim * 0.7, buf, strlen(buf));
		for (i=0;i<kNumCities;i++) {
		  if (liv[i]) {
			 city[i].alive = 1;
			 AddScore(dpy, window, cmap, xlim, ylim, 100 * level);
			 DrawCities(dpy, window, cmap, xlim, ylim);
			 XSync(dpy, False);
                         screenhack_handle_events(dpy);
			 usleep(kCityPause);
		  }
		}
	 }
	 else {
		/* we're dead */
                screenhack_handle_events(dpy);
		sleep(3);
                screenhack_handle_events(dpy);
		/* start new */
		gamez++;
		Improve();
		for (i=0;i<kNumCities;i++)
		  city[i].alive = 1;
		level = 0;
		loop = 1;
		score = 0;
		nextBonus = kFirstBonus;
		numBonus = 0;
		DrawCities(dpy, window, cmap, xlim, ylim);
	 }
  }

  /* do free city part */
  if (freecity && sumlive < 5) {
	 int ncnt = random() % (5 - sumlive) + 1;
	 for (i=0;i<kNumCities;i++)
		if (!city[i].alive)
		  if (!--ncnt)
			 city[i].alive = 1;
	 strcpy(buf, "Bonus City");
	 width = XTextWidth(font, buf, strlen(buf));
	 XDrawString(dpy, window, level_gc, xlim / 2 - width / 2, ylim / 4, buf, strlen(buf));
	 DrawCities(dpy, window, cmap, xlim, ylim);
	 XSync(dpy, False);
         screenhack_handle_events(dpy);
	 sleep(1);
  }

  XFillRectangle(dpy, window, erase_gc,
					  0, 0, xlim, ylim - 100);
  
  if (!bround)
	 level++;
  if (level == 1) {
	 nextBonus = kFirstBonus;
  }

  if (level > 3 && (level % 5 == 1)) {
	 if (bround) {
		bround = 0;
		DrawCities(dpy, window, cmap, xlim, ylim);
	 }
	 else {
		/* bonus round */
		bround = 1;
		levMissiles = 20 + level * 10;
		levFreq = 10;
		for (i=0;i<kNumCities;i++)
		  blive[i] = city[i].alive;
		sprintf(buf, "Bonus Round");
		width = XTextWidth(font, buf, strlen(buf));
		XDrawString(dpy, window, level_gc, xlim / 2 - width / 2, ylim / 2 - font_height(font) / 2, buf, strlen(buf));
		XSync(dpy, False);
                screenhack_handle_events(dpy);
		sleep(1);
		XFillRectangle(dpy, window, erase_gc,
							0, 0, xlim, ylim - 100);
	 }
  }

 END_LEVEL: ;

  if (!bround) {
	 levMissiles = 5 + level * 3;
	 if (level > 5)
		levMissiles += level * 5;
	 /*  levMissiles = 2; */
	 levFreq = 120 - level * 5;
	 if (levFreq < 30)
		levFreq = 30;
  }

  /* ready to fire */
  lastLaser = 0;
}

static void penetrate(Display *dpy, Window window, Colormap cmap)
{
  XWindowAttributes xgwa;
  static int xlim, ylim;

  XGetWindowAttributes(dpy, window, &xgwa);
  xlim = xgwa.width;
  ylim = xgwa.height;

  /* see if just started */
  if (loop == 0) {
	 if (smart) {
		choosypersen = econpersen = carefulpersen = 100;
		lrate = kMinRate; aim = 1;
	 }
	 NewLevel(dpy, window, cmap, xlim, ylim);
	 DrawScore(dpy, window, cmap, xlim, ylim);
  }

  loop++;

  if (levMissiles == 0) {
	 /* see if anything's still on the screen, to know when to end level */
	 int i;
	 for (i=0;i<kMaxMissiles;i++)
		if (missile[i].alive)
		  goto END_CHECK;
	 for (i=0;i<kMaxBooms;i++)
		if (boom[i].alive)
		  goto END_CHECK;
	 for (i=0;i<kMaxLasers;i++)
		if (laser[i].alive)
		  goto END_CHECK;
	 /* okay, nothing's alive, start end of level countdown */
         screenhack_handle_events(dpy);
	 sleep(kLevelPause);
	 NewLevel(dpy, window, cmap, xlim, ylim);
	 return;
  END_CHECK: ;
  }
  else if ((random() % levFreq) == 0) {
	 launch(xlim, ylim, dpy, cmap, -1);
	 levMissiles--;
  }

  if (loop - lastLaser >= lrate) {
	 if (fire(xlim, ylim, dpy, window, cmap))
		lastLaser = loop;
  }

  XSync(dpy, False);
  screenhack_handle_events(dpy);
  if (kSleepTime)
	 usleep(kSleepTime);

  if ((loop & 7) == 0)
	 DrawCities(dpy, window, cmap, xlim, ylim);
  LoopMissiles(dpy, window, cmap, xlim, ylim);
  LoopLasers(dpy, window, cmap, xlim, ylim);
  LoopBooms(dpy, window, cmap, xlim, ylim);
}

char *progclass = "Penetrate";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*bgrowth:	5",
  "*lrate:	80",
  "*geometry:	800x500",
  0
};

XrmOptionDescRec options [] = {
  { "-bgrowth",		".bgrowth",	XrmoptionSepArg, 0 },
  { "-lrate",		".lrate",	XrmoptionSepArg, 0 },
	{"-smart", ".smart", XrmoptionIsArg,0},
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  Colormap cmap = init_penetrate(dpy, window);
  while (1)
    penetrate(dpy, window, cmap);
}
