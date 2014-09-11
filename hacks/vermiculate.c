/*
 *  @(#) vermiculate.c
 *  @(#) Copyright (C) 2001 Tyler Pierce (tyler@alumni.brown.edu)
 *  The full program, with documentation, is available at:
 *    http://freshmeat.net/projects/fdm
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <math.h>
#include "screenhack.h"

#define degs 360
#define degs2 (degs/2)
#define degs4 (degs/4)
#define degs8 (degs/8)
#define dtor 0.0174532925	/*  pi / degs2; */
#define thrmax 120
#define tailmax (thrmax * 2 + 1)
#define tmodes '7'
#define ymax (st->hei - 1)
#define ymin 0
#define xmax (st->wid - 1)
#define xmin 0
#define rlmax 200
#define SPEEDINC 10
#define SPEEDMAX 1000
#define wraparound(VAL,LOWER,UPPER) { 	\
		    if (VAL >= UPPER)	\
		      VAL -= UPPER - LOWER;	\
		    else if (VAL < LOWER)	\
		      VAL += UPPER - LOWER; }
#define arrcpy(DEST,SRC) memcpy (DEST, SRC, sizeof(DEST))

typedef double real;
typedef unsigned char banktype[thrmax];

typedef struct linedata
{
  int deg, spiturn, turnco, turnsize;
  unsigned char col;
  Bool dead;

  char orichar;
  real x, y;
  int tmode, tsc, tslen, tclim, otslen, ctinc, reclen, recpos, circturn, prey,
    slice;
  int xrec[rlmax + 1], yrec[rlmax + 1];
  int turnseq[50];
  Bool filled, killwalls, vhfollow,
    selfbounce, tailfollow, realbounce, little;
}
linedata;

static const struct stringAndSpeed
{
  char *str;
  int speed;
}
sampleStrings[] =
{
/*  { "]]]]]]]]7ces123400-9-8#c123456#s9998880004#ma3#car9ma6#c-#r1", 600} , 
	{ "bmarrrr#c1234#lc5678#lyet]", 600} , */
  { "AEBMN222222223#CAR9CAD4CAOV", 150} ,
  { "mn333#c23#f1#]]]]]]]]]]]3bc9#r9#c78#f9#ma4#", 600} ,
  { "AEBMN22222#CAD4CAORc1#f2#c1#r6", 100} , 
/*  { "mn6666666#c1i#f1#y2#sy2#vyety1#ry13i#l", 40} , */
  { "aebmnrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr#", 500} , 
/*  { "bg+++++++++++++++++++++++#mnrrrrrrrrrrrrrrrrrrrrrrr#y1#k", 500},  */
/*  { "BMN22222223#CAD4CAOVYAS", 150}, */
/*  { "aebmnrrrrrrrrrrrrrrrr#yaryakg--#", 100} , */
  { "mn6rrrrrrrrrrrrrrr#by1i#lcalc1#fnyav" , 200 } ,
  { "mn1rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr#by1i#lcalc1#fn", 2000 },
  { "baeMn333333333333333333333333#CerrYerCal", 800 },
  { "baeMn1111111111111111111111111111111111111111111111111111111111#Cer9YesYevYerCal", 1200 },
  { "baMn111111222222333333444444555555#Ct1#lCt2#lCt3#lCt4#lCt5#lCerrYerYet", 1400 },
  { "baMn111111222222333333444444555555#Ct1#lCt2#lCt3#lCt4#lCt5#lCerrYerYetYt1i#lYt1i#sYt1#v", 1400 }
};

struct state {
  Display *dpy;
  Window window;

  GC mygc;
  Colormap mycmap;
  XWindowAttributes xgwa;
  XColor mycolors[tailmax];

  int hei, wid, speed;
  Bool erasing, cleared, autopal;
  char *instring;
  int max_ticks;

  real sinof[degs], cosof[degs], tanof[degs];
  unsigned char *point;

  linedata thread[thrmax];
  banktype bank;
  int bnkt;
  int boxw, boxh, curviness, gridden, ogd, bordcorn;
  unsigned char bordcol, threads;
  char ch, boolop;

  int reset_p;
  int cyc;
};



static void
consume_instring(struct state *st);

static Bool
wasakeypressed (struct state *st)
{
  if (*st->instring != 0)
    return True;
  else
    return False;
}

static char
readkey (struct state *st)
{
  char readkey_result;
  if (*st->instring == 0)
    {
      readkey_result = '#';
    }
  else
    {
      readkey_result = *st->instring;
      st->instring++;
    };
  return toupper (readkey_result);
}

static unsigned int
random1 (unsigned int i)
{
  return (ya_random () % i);
}

static unsigned long
waitabit (struct state *st)
{
  int result = 0;
  st->cyc += st->threads;
  while (st->cyc > st->speed)
    {
      result += 10000;
      st->cyc -= st->speed;
    }
  return result;
}

static void
clearscreen (struct state *st)
{
  XClearWindow (st->dpy, st->window);
  memset (st->point, 0, st->wid * st->hei);
}

static void
sp (struct state *st, int x, int y, int c)
{
  XSetForeground (st->dpy, st->mygc, st->mycolors[c].pixel);
  XDrawPoint (st->dpy, st->window, st->mygc, x, y);
  st->point[(st->wid * y) + x] = c;
}

static int
gp (struct state *st, int x, int y)
{
  return st->point[(st->wid * y) + x];
}

static void
redraw (struct state *st, int x, int y, int width, int height)
{
  int xc, yc;
  for (xc = x; xc <= x + width - 1; xc++)
    for (yc = y; yc <= y + height - 1; yc++)
      if (st->point[st->wid * yc + xc] != 0)
	sp (st, xc, yc, st->point[st->wid * yc + xc]);
}

static void
palupdate (struct state *st, Bool forceUpdate)
{
  if (forceUpdate || *st->instring == 0)
    {
      redraw (st, xmin, ymin, st->wid, st->hei);
    }
}

static void
randpal (struct state *st)
{
  int ncolors = tailmax - 1;
  make_random_colormap (st->xgwa.screen, st->xgwa.visual, st->mycmap,
                        &st->mycolors[1], &ncolors, True, True, 0, True);
  if (ncolors < tailmax - 1)
    {
      int c;
      for (c = 1; c < tailmax; c++)
	st->mycolors[c].pixel = WhitePixel (st->dpy, DefaultScreen (st->dpy));
    }
}

static void
gridupdate (struct state *st, Bool interruptible)
{
  int x, y;
  if (st->gridden > 0)
    for (x = 0; x <= xmax && !(wasakeypressed (st) && interruptible); x += st->boxw)
      for (y = 0; y <= ymax; y += st->boxh)
	{
	  if (random1 (15) < st->gridden)
	    {
#define lesser(A,B) ( ((A)<(B)) ? (A) : (B) )
	      int max = lesser (x + st->boxw, xmax);
	      int xc;
	      for (xc = x; xc <= max; xc++)
		sp (st, xc, y, 1);
	    }
	  if (random1 (15) < st->gridden)
	    {
	      int max = lesser (y + st->boxh, ymax);
	      int yc;
	      for (yc = y; yc <= max; yc++)
		sp (st, x, yc, 1);
	    }
	}
}

static void
bordupdate (struct state *st)
{
  int xbord, ybord;

  if (st->bordcorn == 0 || st->bordcorn == 1)
    ybord = ymin;
  else
    ybord = ymax;
  if (st->bordcorn == 0 || st->bordcorn == 3)
    xbord = xmin;
  else
    xbord = xmax;
  {
    int x, y;
    for (x = xmin; x <= xmax; x++)
      sp (st, x, ybord, st->bordcol);
    for (y = ymin; y <= ymax; y++)
      sp (st, xbord, y, st->bordcol);
  }
}

static Bool
inbank (struct state *st, unsigned char thr)
{
  int c;
  if (st->bnkt > 0)
    for (c = 1; c <= st->bnkt; c++)
      if (st->bank[c - 1] == thr)
	return True;
  return False;
}

static void
pickbank (struct state *st)
{
  unsigned char thr = 1;
  st->bnkt = 0;
  st->ch = '\0';
  do
    {
      while (inbank (st, thr))
	thr = thr % st->threads + 1;

      palupdate (st, False);
      st->ch = readkey (st);
      palupdate (st, False); 
      switch (st->ch)
	{
	case '+':
	case '-':
	  do
	    {
	      if (st->ch == '+')
		thr++;
	      else
		thr--;
	      wraparound (thr, 1, st->threads + 1);
	    }
	  while (inbank (st, thr));
	  break;
	case ' ':
	  st->bank[++st->bnkt - 1] = thr;
	  break;
	case '1': case '2': case '3':
        case '4': case '5': case '6':
        case '7': case '8': case '9':

	  st->bank[++st->bnkt - 1] = st->ch - '0';
	  if (st->bank[st->bnkt - 1] > st->threads)
	    st->bnkt--;
	  break;
	case 'I':
	  {
	    banktype tbank;
	    int tbankt = 0;
	    int c;
	    for (c = 1; c <= st->threads; c++)
	      if (!inbank (st, c))
		tbank[++tbankt - 1] = c;
	    st->bnkt = tbankt;
	    arrcpy (st->bank, tbank);
	  }
	  break;
	case 'T':
	  st->ch = readkey (st);
	  switch (st->ch)
	    {
	    case '1': case '2': case '3':
            case '4': case '5': case '6':
            case '7': case '8': case '9':

	      {
		int c;
		for (c = 1; c <= st->threads; c++)
		  if (st->thread[c - 1].tmode == st->ch - '0')
		    st->bank[++st->bnkt - 1] = c;
	      }
	      break;
	    }
	  break;
	case 'A':
	  for (st->bnkt = 1; st->bnkt <= st->threads; st->bnkt++)
	    st->bank[st->bnkt - 1] = st->bnkt;
	  st->bnkt = st->threads;
	  break;
	case 'E':
	  for (st->bnkt = 1; st->bnkt <= thrmax; st->bnkt++)
	    st->bank[st->bnkt - 1] = st->bnkt;
	  st->bnkt = thrmax;
	  break;
	}
    }
  while (!(st->bnkt >= st->threads || st->ch == 'N' || st->ch == '\15' || st->ch == '#'));
  if (st->bnkt == 0 && st->ch != 'N')
    {
      st->bnkt = 1;
      st->bank[0] = thr;
    }
  palupdate (st, False);
}

static void
bankmod (char boolop, Bool * Bool_)
{
  switch (boolop)
    {
    case 'T':
      *Bool_ = !*Bool_;
      break;
    case 'Y':
      *Bool_ = True;
      break;
    case 'N':
      *Bool_ = False;
      break;
    }
}

static void
newonscreen (struct state *st, unsigned char thr)
{
  linedata *LP = &st->thread[thr - 1];
  LP->filled = False;
  LP->dead = False;
  LP->reclen = (LP->little) ? 
	random1 (10) + 5 : random1 (rlmax - 30) + 30;
  LP->deg = random1 (degs);
  LP->y = random1 (st->hei);
  LP->x = random1 (st->wid);
  LP->recpos = 0;
  LP->turnco = 2;
  LP->turnsize = random1 (4) + 2;
}

static void
firstinit (struct state *st, unsigned char thr)
{
  linedata *LP = &st->thread[thr - 1];
  LP->col = thr + 1;
  LP->prey = 0;
  LP->tmode = 1;
  LP->slice = degs / 3;
  LP->orichar = 'R';
  LP->spiturn = 5;
  LP->selfbounce = False;
  LP->realbounce = False;
  LP->vhfollow = False;
  LP->tailfollow = False;
  LP->killwalls = False;
  LP->little = False;
  LP->ctinc = random1 (2) * 2 - 1;
  LP->circturn = ((thr % 2) * 2 - 1) * ((thr - 1) % 7 + 1);
  LP->tsc = 1;
  LP->tslen = 6;
  LP->turnseq[0] = 6;
  LP->turnseq[1] = -6;
  LP->turnseq[2] = 6;
  LP->turnseq[3] = 6;
  LP->turnseq[4] = -6;
  LP->turnseq[5] = 6;
  LP->tclim = (unsigned char) (((real) degs) / 2 / 12);
}

static void
maininit (struct state *st)
{
  if (!st->instring)
    {
      int n = random1 (sizeof (sampleStrings) / sizeof (sampleStrings[0]));
      st->instring = sampleStrings[n].str;
      st->speed = sampleStrings[n].speed;
    }
  if (st->speed == 0)
    {
	  st->speed = 200;
    }
  st->boxh = 10;
  st->boxw = 10;
  st->gridden = 0;
  st->bordcorn = 0;
  st->threads = 4;
  st->curviness = 30;
  st->bordcol = 1;
  st->ogd = 8;
  st->ch = '\0';
  st->erasing = True;
  {
    unsigned char thr;
    for (thr = 1; thr <= thrmax; thr++)
      {
	firstinit (st, thr);
	newonscreen (st, thr);
      }
  }
  {
    int d;
    for (d = degs - 1; d >= 0; d--)
      {
	st->sinof[d] = sin (d * dtor);
	st->cosof[d] = cos (d * dtor);
	if (d % degs4 == 0)
	  st->tanof[d] = st->tanof[d + 1];
	else
	  st->tanof[d] = tan (d * dtor);
      }
  }
  randpal (st);
}

static Bool
move (struct state *st, unsigned char thr)
{
  linedata *LP = &st->thread[thr - 1];
  if (LP->dead)
    return (False);
  if (LP->prey == 0)
    switch (LP->tmode)
      {
      case 1:
	LP->deg += random1 (2 * LP->turnsize + 1) - LP->turnsize;
	break;
      case 2:
	if (LP->slice == degs || LP->slice == degs2 || LP->slice == degs4)
	  {
	    if (LP->orichar == 'D')
	      {
		if (LP->deg % degs4 != degs8)
		  LP->deg = degs4 * random1 (4) + degs8;
	      }
	    else if (LP->orichar == 'V')
	      if (LP->deg % degs4 != 0)
		LP->deg = degs4 * random1 (4);
	  }
	if (random1 (100) == 0)
	  {
	    if (LP->slice == 0)
	      LP->deg = LP->deg - degs4 + random1 (degs2);
	    else
	      LP->deg += (random1 (2) * 2 - 1) * LP->slice;
	  }
	break;
      case 3:
	LP->deg += LP->circturn;
	break;
      case 4:
	if (abs (LP->spiturn) > 11)
	  LP->spiturn = 5;
	else
	  LP->deg += LP->spiturn;
	if (random1 (15 - abs (LP->spiturn)) == 0)
	  {
	    LP->spiturn += LP->ctinc;
	    if (abs (LP->spiturn) > 10)
	      LP->ctinc *= -1;
	  }
	break;
      case 5:
	LP->turnco = abs (LP->turnco) - 1;
	if (LP->turnco == 0)
	  {
	    LP->turnco = st->curviness + random1 (10);
	    LP->circturn *= -1;
	  }
	LP->deg += LP->circturn;
	break;
      case 6:
	if (abs (LP->turnco) == 1)
	  LP->turnco *= -1 * (random1 (degs2 / abs (LP->circturn)) + 5);
	else if (LP->turnco == 0)
	  LP->turnco = 2;
	else if (LP->turnco > 0)
	  {
	    LP->turnco--;
	    LP->deg += LP->circturn;
	  }
	else
	  LP->turnco++;
	break;
      case 7:
	LP->turnco++;
	if (LP->turnco > LP->tclim)
	  {
	    LP->turnco = 1;
	    LP->tsc = (LP->tsc % LP->tslen) + 1;
	  }
	LP->deg += LP->turnseq[LP->tsc - 1];
	break;
      }
  else
    {
      int desdeg;
      real dy, dx;
      if (LP->tailfollow || LP->prey == thr)
	{
	  dx = st->thread[LP->prey - 1].xrec[st->thread[LP->prey - 1].recpos] - LP->x;
	  dy = st->thread[LP->prey - 1].yrec[st->thread[LP->prey - 1].recpos] - LP->y;
	}
      else
	{
	  dx = st->thread[LP->prey - 1].x - LP->x;
	  dy = st->thread[LP->prey - 1].y - LP->y;
	}
      desdeg =
	(LP->vhfollow) ?
	((fabs (dx) > fabs (dy)) ?
	 ((dx > 0) ?
	  0 * degs4
	  :
	  2 * degs4)
	 :
	 ((dy > 0) ?
	  1 * degs4
	  :
	  3 * degs4))
	:
	((dx > 0) ?
	 ((dy > 0) ?
	  1 * degs8 : 7 * degs8) : ((dy > 0) ? 3 * degs8 : 5 * degs8));
      if (desdeg - desdeg % degs4 != LP->deg - LP->deg % degs4
	  || LP->vhfollow)
	{
	  if (!LP->vhfollow)
           { 
              /* Using atan2 here doesn't seem to slow things down: */
              desdeg = atan2 (dy, dx) / dtor;
              wraparound (desdeg, 0, degs);
	   }
	  if (abs (desdeg - LP->deg) <= abs (LP->circturn))
	    LP->deg = desdeg;
	  else
	    LP->deg +=
	      (desdeg > LP->deg) ?
	      ((desdeg - LP->deg > degs2) ?
	       -abs (LP->circturn) : abs (LP->circturn))
	      : ((LP->deg - desdeg > degs2) ?
		 abs (LP->circturn) : -abs (LP->circturn));
	}
      else
	LP->deg +=
	  (st->tanof[LP->deg] >
	   dy / dx) ? -abs (LP->circturn) : abs (LP->circturn);
    }

  wraparound (LP->deg, 0, degs);
  {
    unsigned char oldcol;
    real oldy = LP->y, oldx = LP->x;
    LP->x += st->cosof[LP->deg];
    wraparound (LP->x, xmin, xmax + 1);
    LP->y += st->sinof[LP->deg];
    wraparound (LP->y, ymin, ymax + 1);
#define xi ((int) LP->x)
#define yi ((int) LP->y)

    oldcol = gp (st, xi, yi);
    if (oldcol != 0)
      {
	Bool vertwall = False, horiwall = False;
	if (oldcol == 1 && ((LP->killwalls && st->gridden > 0) || LP->realbounce))
	  {
	    vertwall = (gp (st, xi, (int) oldy) == 1);
	    horiwall = (gp (st, (int) oldx, yi) == 1);
	  }
	if (oldcol == 1 && LP->realbounce && (vertwall || horiwall))
	  {
	    if (vertwall)
	      LP->deg = -LP->deg + degs2;
	    else
	      LP->deg = -LP->deg;
	  }
	else
	  {
	    if ((oldcol != LP->col && LP->realbounce)
		|| (oldcol == LP->col && LP->selfbounce))
	      LP->deg += degs4 * (random1 (2) * 2 - 1);
	    else if (oldcol != LP->col)
	      LP->deg += degs2;
	  }
	if (LP->killwalls && st->gridden > 0 && oldcol == 1)
	  {
	    if (vertwall && xi + 1 <= xmax)
	      {
		int yy;
		for (yy = yi - yi % st->boxh;
		     yy <= yi - yi % st->boxh + st->boxh && yy <= ymax; yy++)
		  if (gp (st, xi + 1, yy) != 1 || yy == ymax)
		    sp (st, xi, yy, 0);
	      }
	    if (horiwall && yi + 1 <= ymax)
	      {
		int xx;
		for (xx = xi - xi % st->boxw;
		     xx <= xi - xi % st->boxw + st->boxw && xx <= xmax; xx++)
		  if (gp (st, xx, yi + 1) != 1 || xx == xmax)
		    sp (st, xx, yi, 0);
	      }
	  }
	if (oldcol != LP->col || LP->selfbounce)
	  {
	    LP->x = oldx;
	    LP->y = oldy;
	  }
	wraparound (LP->deg, 0, degs);
      }
  }

  sp (st, xi, yi, LP->col);
  if (LP->filled)
    {
      if (st->erasing)
	sp (st, LP->xrec[LP->recpos], LP->yrec[LP->recpos], 0);
      else
	sp (st, LP->xrec[LP->recpos], LP->yrec[LP->recpos], LP->col + thrmax);
    }
  LP->yrec[LP->recpos] = yi;
  LP->xrec[LP->recpos] = xi;
  if (LP->recpos == LP->reclen - 1)
    LP->filled = True;
  if (LP->filled && !st->erasing)
    {
      int co = LP->recpos;
      LP->dead = True;
      do
	{
	  int nextco = co + 1;
	  wraparound (nextco, 0, LP->reclen);
	  if (LP->yrec[co] != LP->yrec[nextco]
	      || LP->xrec[co] != LP->xrec[nextco])
	    LP->dead = False;
	  co = nextco;
	}
      while (!(!LP->dead || co == LP->recpos));
    }
  LP->recpos++;
  wraparound (LP->recpos, 0, LP->reclen);
  return (!LP->dead);
}

static unsigned long
vermiculate_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int had_instring = (st->instring != 0);
  int tick = 0;
  int this_delay = 0;
  int loop = 0;

  
 AGAIN:
  if (st->reset_p) 
    {
      st->reset_p = 0;

      clearscreen (st);
      {
	unsigned char thr;
	for (thr = 1; thr <= st->threads; thr++)
	  newonscreen (st, thr);
      }
      if (st->autopal)
	{
	  randpal (st);
	  palupdate (st, False);
	}
      bordupdate (st);
      gridupdate (st, False);
    }

  {
    Bool alltrap = True;
    unsigned char thr;
    for (thr = 1; thr <= st->threads; thr++)
      if (move (st, thr))
        alltrap = False;
    if (alltrap)	/* all threads are trapped */
      st->reset_p = True;
    if (st->speed != SPEEDMAX)
      this_delay = waitabit (st);
  }

  if (tick++ > st->max_ticks && !had_instring)
    {
      tick = 0;
      st->instring = 0;
      maininit(st);
      st->reset_p = True;
      st->autopal = False;
    }

  if (this_delay == 0 && loop++ < 1000)
    goto AGAIN;

  return this_delay;
}

static void *
vermiculate_init (Display *d, Window w)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = d;
  st->window = w;
  st->reset_p = 1;
  st->instring = get_string_resource (st->dpy, "instring", "Instring");
  if (st->instring && !*st->instring)
    st->instring = 0;

  st->max_ticks = get_integer_resource (st->dpy, "ticks", "Integer");
  {
    int temp = get_integer_resource (st->dpy, "speed", "Speed");
    if (temp != 0)
      st->speed = temp;
  }

  st->mycolors[0].pixel = BlackPixel (st->dpy, DefaultScreen (st->dpy));

  XSetWindowBackground (st->dpy, st->window,
			BlackPixel (st->dpy, DefaultScreen (st->dpy)));
  {
    XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
    st->wid = st->xgwa.width;
    st->hei = st->xgwa.height;
    st->mycmap = st->xgwa.colormap;
  }
  {
    XGCValues mygcv;
    st->mygc = XCreateGC (st->dpy, st->window, 0, &mygcv);
  }

  st->point = (unsigned char *) calloc (1, st->wid * st->hei);
  maininit (st);
  palupdate (st, True);
  consume_instring(st);
  return st;
}


static void
vermiculate_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->wid = w;
  st->hei = h;
  free (st->point);
  st->point = (unsigned char *) calloc (1, st->wid * st->hei);
}

static Bool
vermiculate_event (Display *dpy, Window window, void *closure, XEvent *event)
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
consume_instring(struct state *st)
{
  char boolop;

	  while (wasakeypressed (st))
	    {
	      st->ch = readkey (st);
	      switch (st->ch)
		{
		case 'M':
		  st->ch = readkey (st);
		  switch (st->ch)
		    {
		    case 'A':
		    case 'N':
		      {
			unsigned char othreads = st->threads;
			if (st->ch == 'N')
			  st->threads = 0;
			do
			  {
			    st->ch = readkey (st);
			    switch (st->ch)
			      {
                              case '1': case '2': case '3':
                              case '4': case '5': case '6':
                              case '7': case '8': case '9':
				st->thread[++st->threads - 1].tmode = st->ch - '0';
				break;
			      case 'R':
				st->thread[++st->threads - 1].tmode =
				  random1 (tmodes - '0') + 1;
				break;
			      }
			  }
			while (!(st->ch == '\15' || st->ch == '#'
				 || st->threads == thrmax));
			if (st->threads == 0)
			  st->threads = othreads;
			st->reset_p = 1;
		      }
		      break;
		    }
		  break;
		case 'C':
		  pickbank (st);
		  if (st->bnkt > 0)
		    {
		      st->ch = readkey (st);
		      switch (st->ch)
			{
			case 'D':
			  st->ch = readkey (st);
			  switch (st->ch)
			    {
                            case '1': case '2': case '3':
                            case '4': case '5': case '6':
                            case '7': case '8': case '9':
/* Careful!  The following macro needs to be at the beginning of any
block in which it's invoked, since it declares variables: */
#define forallinbank(LDP) linedata *LDP; int bankc; \
		for (bankc = 1;	\
		((bankc <= st->bnkt) ? ( \
			(LDP = &st->thread[st->bank[bankc - 1] - 1],	1) \
			) : 0) ; bankc++)
			      {
				forallinbank (L) L->slice = degs / (st->ch - '0');
			      }
			      break;
			    case 'M':
			      {
				forallinbank (L) L->slice = 0;
			      }
			      break;
			    }
			  break;
			case 'S':
			  {
			    forallinbank (L)
			    {
			      L->otslen = L->tslen;
			      L->tslen = 0;
			    }
			  }
			  do
			    {
			      char oldch = st->ch;
			      st->ch = readkey (st);
			      {
				forallinbank (L)
				{
				  switch (st->ch)
				    {
                                    case '0':
                                    case '1': case '2': case '3':
                                    case '4': case '5': case '6':
                                    case '7': case '8': case '9':
				      L->tslen++;
				      L->turnseq[L->tslen - 1] = st->ch - '0';
				      if (oldch == '-')
					L->turnseq[L->tslen - 1] *= -1;
				      if (bankc % 2 == 0)
					L->turnseq[L->tslen - 1] *= -1;
				      break;
				    }
				}
			      }
			    }
			  while (!(st->ch == '\15' || st->ch == '#'
				   || st->thread[st->bank[0] - 1].tslen == 50));
			  {
			    forallinbank (L)
			    {
			      int seqSum = 0, c;

			      if (L->tslen == 0)
				L->tslen = L->otslen;
			      for (c = 1; c <= L->tslen; c++)
				seqSum += L->turnseq[c - 1];
			      if (seqSum == 0)
				L->tclim = 1;
			      else
				L->tclim =
				  (int) (((real) degs2) / abs (seqSum));
			      L->tsc = random1 (L->tslen) + 1;
			    }
			  }
			  break;
			case 'T':
			  {
			    st->ch = readkey (st);
			    {
			      forallinbank (L)
			      {
				switch (st->ch)
				  {
                                  case '1': case '2': case '3':
                                  case '4': case '5': case '6':
                                  case '7': case '8': case '9':
				    L->tmode = st->ch - '0';
				    break;
				  case 'R':
				    L->tmode = random1 (tmodes - '0') + 1;
				    break;
				  }
			      }
			    }
			  }
			  break;
			case 'O':
			  st->ch = readkey (st);
			  {
			    forallinbank (L) L->orichar = st->ch;
			  }
			  break;
			case 'F':
			  {
			    banktype fbank;
			    arrcpy (fbank, st->bank);
			    {
			      int fbnkt = st->bnkt;
			      int bankc;
			      pickbank (st);
			      for (bankc = 1; bankc <= fbnkt; bankc++)
				{
				  linedata *L = &st->thread[fbank[bankc - 1] - 1];
				  if (st->ch == 'N')
				    L->prey = 0;
				  else
				    L->prey = st->bank[0 + (bankc - 1) % st->bnkt];
				}
			    }
			  }
			  break;
			case 'L':
			  {
			    forallinbank (L) L->prey = st->bank[bankc % st->bnkt];
			  }
			  break;
			case 'R':
			  st->ch = readkey (st);
			  {
			    forallinbank (L) switch (st->ch)
			      {
                              case '1': case '2': case '3':
                              case '4': case '5': case '6':
                              case '7': case '8': case '9':
				L->circturn = 10 - (st->ch - '0');
				break;
			      case 'R':
				L->circturn = random1 (7) + 1;
				break;
			      }
			  }
			  break;
			}
		    }
		  break;
		case 'T':
		case 'Y':
		case 'N':
		  boolop = st->ch;
		  pickbank (st);
		  if (st->bnkt > 0)
		    {
		      st->ch = readkey (st);
		      {
			forallinbank (L)
			{
			  switch (st->ch)
			    {
			    case 'S':
			      bankmod (boolop, &L->selfbounce);
			      break;
			    case 'V':
			      bankmod (boolop, &L->vhfollow);
			      break;
			    case 'R':
			      bankmod (boolop, &L->realbounce);
			      break;
			    case 'L':
			      bankmod (boolop, &L->little);
			      st->cleared = True;
			      break;
			    case 'T':
			      bankmod (boolop, &L->tailfollow);
			      break;
			    case 'K':
			      bankmod (boolop, &L->killwalls);
			      break;
			    }
			}
		      }
		    }
		  break;
		case 'R':
		  if (st->bordcol == 1)
		    {
		      st->bordcol = 0;
		      bordupdate (st);
		      st->bordcorn = (st->bordcorn + 1) % 4;
		      st->bordcol = 1;
		      bordupdate (st);
		    }
		  break;
                case '1': case '2': case '3':
                case '4': case '5': case '6':
                case '7': case '8': case '9':
		  {
		    int c;
		    for (c = 1; c <= thrmax; c++)
		      st->thread[c - 1].tmode = st->ch - '0';
		  }
		  break;
		case '\40':
		  st->cleared = True;
		  break;
		case 'E':
		  st->erasing = !st->erasing;
		  break;
		case 'P':
		  randpal (st);
		  palupdate (st, True);
		  break;
		case 'G':
		  {
		    char dimch = 'B';
		    Bool gridchanged = True;
		    if (st->gridden == 0)
		      st->gridden = st->ogd;
		    do
		      {
			int msize = 0;
			if (gridchanged)
			  {
			    clearscreen (st);
			    gridupdate (st, True);
			  }
			st->ch = readkey (st);
			gridchanged = True;
			switch (st->ch)
			  {
			  case '+':
			    msize = 1;
			    break;
			  case '-':
			    msize = -1;
			    break;
			  case ']':
			    if (st->gridden < 15)
			      st->gridden++;
			    break;
			  case '[':
			    if (st->gridden > 0)
			      st->gridden--;
			    break;
			  case 'O':
			    st->ogd = st->gridden;
			    st->gridden = 0;
			    break;
			  case 'S':
			    st->boxw = st->boxh;
			  case 'W':
			  case 'H':
			  case 'B':
			    dimch = st->ch;
			    break;
			  default:
			    gridchanged = False;
			  }
			if (dimch == 'W' || dimch == 'B')
			  st->boxw += msize;
			if (dimch == 'H' || dimch == 'B')
			  st->boxh += msize;
			if (st->boxw == 0)
			  st->boxw = 1;
			if (st->boxh == 0)
			  st->boxh = 1;
		      }
		    while (!(st->ch == '\15' || st->ch == '#' || st->ch == 'O'));
		    st->cleared = True;
		  }
		  break;
		case 'A':
		  st->autopal = !st->autopal;
		  break;
		case 'B':
		  st->bordcol = 1 - st->bordcol;
		  bordupdate (st);
		  break;
		case '-':
		  st->speed -= SPEEDINC;
		  if (st->speed < 1)
		    st->speed = 1;
		  break;
		case '+':
		  st->speed += SPEEDINC;
		  if (st->speed > SPEEDMAX)
		    st->speed = SPEEDMAX;
		  break;
		case '/':
		  if (st->curviness > 5)
		    st->curviness -= 5;
		  break;
		case '*':
		  if (st->curviness < 50)
		    st->curviness += 5;
		  break;
		case ']':
		  if (st->threads < thrmax)
		    newonscreen (st, ++st->threads);
		  break;
		case '[':
		  if (st->threads > 1)
		    {
		      linedata *L = &st->thread[st->threads - 1];
		      int lastpos = (L->filled) ? L->reclen - 1 : L->recpos;
		      int c;
		      for (c = 0; c <= lastpos; c++)
			sp (st, L->xrec[c], L->yrec[c], 0);
		      st->threads--;
		    }
		  break;
		}
	    }
}

static void
vermiculate_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->point)
    {
      free(st->point);
    }
  free (st);
}


static const char *vermiculate_defaults[] = {
  ".background: Black",
  "*ticks: 20000",
  "*fpsSolid:	true",
  "*speed: 0",
  "*instring: ",
#ifdef USE_IPHONE
  "*ignoreRotation: True",
#endif
  0
};

static XrmOptionDescRec vermiculate_options[] = {
  {"-speed", ".speed", XrmoptionSepArg, 0},
  {"-instring", ".instring", XrmoptionSepArg, 0},
  {0, 0, 0, 0}
};


XSCREENSAVER_MODULE ("Vermiculate", vermiculate)
