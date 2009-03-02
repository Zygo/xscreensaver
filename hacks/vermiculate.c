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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>

#ifdef VERMICULATE_STANDALONE
#include "yarandom.h"
#include "usleep.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#else
#include "screenhack.h"
#include "config.h"
#endif /* VERMICULATE_STANDALONE */

#define degs 360
#define degs2 (degs/2)
#define degs4 (degs/4)
#define degs8 (degs/8)
#define dtor 0.0174532925	/*  pi / degs2; */
#define thrmax 120
#define tailmax (thrmax * 2 + 1)
#define tmodes '7'
#define ymax (hei - 1)
#define ymin 0
#define xmax (wid - 1)
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

#ifdef    VERMICULATE_STANDALONE
static XEvent myevent;
static Bool use_root = False;
static unsigned char rgb[256][3];

#else
char *progclass = "Vermiculate";

char *defaults[] = {
  ".ticks: 20000",
  0
};

XrmOptionDescRec options[] = {
  {"-speed", ".speed", XrmoptionSepArg, 0},
  {"-instring", ".instring", XrmoptionSepArg, 0},
  {0, 0, 0, 0}
};
#endif /* VERMICULATE_STANDALONE */

static Display *mydpy;
static Window mywindow;
static GC mygc;
static Colormap mycmap;
static XWindowAttributes xgwa;
static Bool neednewkey = True;
static XColor mycolors[tailmax];

static int hei = 500, wid = 500, speed = 100;
static Bool erasing = True;
static char *instring = 0;
static int max_ticks;

static struct stringAndSpeed
{
  char *str;
  int speed;
}
sampleStrings[] =
{
  { "]]]]]]]]7ces123400-9-8#c123456#s9998880004#ma3#car9ma6#c-#r1", 600} ,
  { "bmarrrr#c1234#lc5678#lyet]", 600} ,
  { "AEBMN222222223#CAR9CAD4CAOV", 150} ,
  { "mn333#c23#f1#]]]]]]]]]]]3bc9#r9#c78#f9#ma4#", 600} ,
  { "AEBMN22222#CAD4CAORc1#f2#c1#r6", 100} ,
/*  { "mn6666666#c1i#f1#y2#sy2#vyety1#ry13i#l", 40} , */
  { "aebmnrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr#", 500} ,
  { "bg+++++++++++++++++++++++#mnrrrrrrrrrrrrrrrrrrrrrrr#y1#k", 500} ,
  { "BMN22222223#CAD4CAOVYAS", 150} ,
/*  { "aebmnrrrrrrrrrrrrrrrr#yaryakg--#", 100} , */
  { "mn6rrrrrrrrrrrrrrr#by1i#lcalc1#fnyav", 200 } ,
  { "mn1rrrrrrrrrrrrrrr#by1i#lcalc1#fn", 200 }
};

static real sinof[degs], cosof[degs], tanof[degs];
static unsigned char *point;

static linedata thread[thrmax];
static banktype bank;
static int bankt, boxw, boxh, curviness, gridden, ogd, bordcorn;
static unsigned char bordcol, threads;
static char ch, boolop;

static Bool
wasakeypressed (void)
{
  if (!neednewkey || *instring != 0)
    return True;
  else
#ifdef VERMICULATE_STANDALONE
    return !(neednewkey =
	     !XCheckWindowEvent (mydpy, mywindow, KeyPressMask, &myevent));
#else
    return False;
#endif /* VERMICULATE_STANDALONE */
}

static char
readkey (void)
{
  char readkey_result;
  if (*instring == 0)
    {
#ifdef VERMICULATE_STANDALONE
      char key_buffer[1];
      KeySym key_sym;
      if (neednewkey)
	XWindowEvent (mydpy, mywindow, KeyPressMask, &myevent);
      XLookupString (&myevent.xkey, key_buffer, 1, &key_sym, NULL);
      readkey_result = key_sym;
#else
      readkey_result = '#';
#endif /* VERMICULATE_STANDALONE */
      neednewkey = True;
    }
  else
    {
      readkey_result = *instring;
      instring++;
    };
  return toupper (readkey_result);
}

static unsigned int
random1 (unsigned int i)
{
  return (ya_random () % i);
}

static void
waitabit (void)
{
  static int cyc = 0;
  cyc += threads;
  while (cyc > speed)
    {
      usleep (10000);
      cyc -= speed;
    }
}

static void
clearscreen (void)
{
  XClearWindow (mydpy, mywindow);
  memset (point, 0, wid * hei);
}

static void
sp (int x, int y, int c)
{
  XSetForeground (mydpy, mygc, mycolors[c].pixel);
  XDrawPoint (mydpy, mywindow, mygc, x, y);
  point[(wid * y) + x] = c;
}

static int
gp (int x, int y)
{
  return point[(wid * y) + x];
}

static void
redraw (int x, int y, int width, int height)
{
  int xc, yc;
  for (xc = x; xc <= x + width - 1; xc++)
    for (yc = y; yc <= y + height - 1; yc++)
      if (point[wid * yc + xc] != 0)
	sp (xc, yc, point[wid * yc + xc]);
}

static void
palupdate (Bool forceUpdate)
{
  if (forceUpdate || *instring == 0)
    {
#ifdef VERMICULATE_STANDALONE
      int colnum;
      for (colnum = 0; colnum < tailmax; colnum++)
	{
	  mycolors[colnum].red = rgb[colnum][0] << 10;
	  mycolors[colnum].green = rgb[colnum][1] << 10;
	  mycolors[colnum].blue = rgb[colnum][2] << 10;
	  mycolors[colnum].flags = DoRed | DoBlue | DoGreen;
	  XAllocColor (mydpy, mycmap, &mycolors[colnum]);
	};
#endif /* VERMICULATE_STANDALONE */
      redraw (xmin, ymin, wid, hei);
    }
}

static void
randpal (void)
{
#ifdef VERMICULATE_STANDALONE
  int co, ro;
  for (co = 1; co <= 255; co++)
    for (ro = 0; ro <= 2; ro++)
      if (co > tailmax)
	rgb[co][ro] = random1 (20);
      else
	rgb[co][ro] = random1 (64);
  for (ro = 0; ro <= 2; ro++)
    rgb[0][ro] = 0;
#else
  int ncolors = tailmax - 1;
  make_random_colormap (mydpy,
			xgwa.visual,
			mycmap, &mycolors[1], &ncolors, True, True, 0, True);
  if (ncolors < tailmax - 1)
    {
      int c;
      for (c = 1; c < tailmax; c++)
	mycolors[c].pixel = WhitePixel (mydpy, DefaultScreen (mydpy));
    }
#endif /* VERMICULATE_STANDALONE */
}

static void
gridupdate (Bool interruptible)
{
  int x, y;
  if (gridden > 0)
    for (x = 0; x <= xmax && !(wasakeypressed () && interruptible); x += boxw)
      for (y = 0; y <= ymax; y += boxh)
	{
	  if (random1 (15) < gridden)
	    {
#define lesser(A,B) ( ((A)<(B)) ? (A) : (B) )
	      int max = lesser (x + boxw, xmax);
	      int xc;
	      for (xc = x; xc <= max; xc++)
		sp (xc, y, 1);
	    }
	  if (random1 (15) < gridden)
	    {
	      int max = lesser (y + boxh, ymax);
	      int yc;
	      for (yc = y; yc <= max; yc++)
		sp (x, yc, 1);
	    }
	}
}

static void
bordupdate (void)
{
  int xbord, ybord;

  if (bordcorn == 0 || bordcorn == 1)
    ybord = ymin;
  else
    ybord = ymax;
  if (bordcorn == 0 || bordcorn == 3)
    xbord = xmin;
  else
    xbord = xmax;
  {
    int x, y;
    for (x = xmin; x <= xmax; x++)
      sp (x, ybord, bordcol);
    for (y = ymin; y <= ymax; y++)
      sp (ybord, y, bordcol);
  }
}

static Bool
inbank (unsigned char thr)
{
  int c;
  if (bankt > 0)
    for (c = 1; c <= bankt; c++)
      if (bank[c - 1] == thr)
	return True;
  return False;
}

static void
pickbank (void)
{
  unsigned char thr = 1;
#ifdef VERMICULATE_STANDALONE
  int co, ro;
  unsigned char orgb[256][3];

  arrcpy (orgb, rgb);
  for (co = 2; co <= tailmax; co++)
    for (ro = 0; ro <= 2; ro++)
      rgb[co][ro] = 25;
#endif /* VERMICULATE_STANDALONE */
  bankt = 0;
  ch = '\0';
  do
    {
      while (inbank (thr))
	thr = thr % threads + 1;
#ifdef VERMICULATE_STANDALONE
      for (co = 1; co <= threads; co++)
	{
	  for (ro = 0; ro <= 2; ro++)
	    rgb[co + 1][ro] = 25;
	  if (inbank (co))
	    for (ro = 0; ro <= 1; ro++)
	      rgb[co + 1][ro] = 60;
	}
      for (ro = 0; ro <= 2; ro++)
	rgb[thr + 1][ro] = 60;
#endif /* VERMICULATE_STANDALONE */
      palupdate (False);
      ch = readkey ();
      palupdate (False);
      switch (ch)
	{
	case '+':
	case '-':
	  do
	    {
	      if (ch == '+')
		thr++;
	      else
		thr--;
	      wraparound (thr, 1, threads + 1);
	    }
	  while (inbank (thr));
	  break;
	case ' ':
	  bank[++bankt - 1] = thr;
	  break;
	case '1'...'9':
	  bank[++bankt - 1] = ch - '0';
	  if (bank[bankt - 1] > threads)
	    bankt--;
	  break;
	case 'I':
	  {
	    banktype tbank;
	    int tbankt = 0;
	    int c;
	    for (c = 1; c <= threads; c++)
	      if (!inbank (c))
		tbank[++tbankt - 1] = c;
	    bankt = tbankt;
	    arrcpy (bank, tbank);
	  }
	  break;
	case 'T':
	  ch = readkey ();
	  switch (ch)
	    {
	    case '1'...tmodes:
	      {
		int c;
		for (c = 1; c <= threads; c++)
		  if (thread[c - 1].tmode == ch - '0')
		    bank[++bankt - 1] = c;
	      }
	      break;
	    }
	  break;
	case 'A':
	  for (bankt = 1; bankt <= threads; bankt++)
	    bank[bankt - 1] = bankt;
	  bankt = threads;
	  break;
	case 'E':
	  for (bankt = 1; bankt <= thrmax; bankt++)
	    bank[bankt - 1] = bankt;
	  bankt = thrmax;
	  break;
	}
    }
  while (!(bankt >= threads || ch == 'N' || ch == '\15' || ch == '#'));
  if (bankt == 0 && ch != 'N')
    {
      bankt = 1;
      bank[0] = thr;
    }
#ifdef VERMICULATE_STANDALONE
  arrcpy (rgb, orgb);
#endif /* VERMICULATE_STANDALONE */
  palupdate (False);
}

static void
bankmod (Bool * Bool_)
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
newonscreen (unsigned char thr)
{
  linedata *LP = &thread[thr - 1];
  LP->filled = False;
  LP->dead = False;
  LP->reclen = (LP->little) ? 
	random1 (10) + 5 : random1 (rlmax - 30) + 30;
  LP->deg = random1 (degs);
  LP->y = random1 (hei);
  LP->x = random1 (wid);
  LP->recpos = 0;
  LP->turnco = 2;
  LP->turnsize = random1 (4) + 2;
}

static void
firstinit (unsigned char thr)
{
  linedata *LP = &thread[thr - 1];
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
maininit (void)
{
  if (!instring)
    {
      int n = random1 (sizeof (sampleStrings) / sizeof (sampleStrings[0]));
      instring = sampleStrings[n].str;
      speed = sampleStrings[n].speed;
    }
  boxh = 10;
  boxw = 10;
  gridden = 0;
  bordcorn = 0;
  threads = 4;
  curviness = 30;
  bordcol = 1;
  ogd = 8;
  ch = '\0';
  erasing = True;
  {
    unsigned char thr;
    for (thr = 1; thr <= thrmax; thr++)
      {
	firstinit (thr);
	newonscreen (thr);
      }
  }
  {
    int d;
    for (d = degs - 1; d >= 0; d--)
      {
	sinof[d] = sin (d * dtor);
	cosof[d] = cos (d * dtor);
	if (d % degs4 == 0)
	  tanof[d] = tanof[d + 1];
	else
	  tanof[d] = tan (d * dtor);
      }
  }
  randpal ();
}

static Bool
move (unsigned char thr)
{
  linedata *LP = &thread[thr - 1];
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
	    LP->turnco = curviness + random1 (10);
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
	  dx = thread[LP->prey - 1].xrec[thread[LP->prey - 1].recpos] - LP->x;
	  dy = thread[LP->prey - 1].yrec[thread[LP->prey - 1].recpos] - LP->y;
	}
      else
	{
	  dx = thread[LP->prey - 1].x - LP->x;
	  dy = thread[LP->prey - 1].y - LP->y;
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
	  (tanof[LP->deg] >
	   dy / dx) ? -abs (LP->circturn) : abs (LP->circturn);
    }

  wraparound (LP->deg, 0, degs);
  {
    unsigned char oldcol;
    real oldy = LP->y, oldx = LP->x;
    LP->x += cosof[LP->deg];
    wraparound (LP->x, xmin, xmax + 1);
    LP->y += sinof[LP->deg];
    wraparound (LP->y, ymin, ymax + 1);
#define xi ((int) LP->x)
#define yi ((int) LP->y)

    oldcol = gp (xi, yi);
    if (oldcol != 0)
      {
	Bool vertwall = False, horiwall = False;
	if (oldcol == 1 && ((LP->killwalls && gridden > 0) || LP->realbounce))
	  {
	    vertwall = (gp (xi, (int) oldy) == 1);
	    horiwall = (gp ((int) oldx, yi) == 1);
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
	if (LP->killwalls && gridden > 0 && oldcol == 1)
	  {
	    if (vertwall && xi + 1 <= xmax)
	      {
		int yy;
		for (yy = yi - yi % boxh;
		     yy <= yi - yi % boxh + boxh && yy <= ymax; yy++)
		  if (gp (xi + 1, yy) != 1 || yy == ymax)
		    sp (xi, yy, 0);
	      }
	    if (horiwall && yi + 1 <= ymax)
	      {
		int xx;
		for (xx = xi - xi % boxw;
		     xx <= xi - xi % boxw + boxw && xx <= xmax; xx++)
		  if (gp (xx, yi + 1) != 1 || xx == xmax)
		    sp (xx, yi, 0);
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

  sp (xi, yi, LP->col);
  if (LP->filled)
    {
      if (erasing)
	sp (LP->xrec[LP->recpos], LP->yrec[LP->recpos], 0);
      else
	sp (LP->xrec[LP->recpos], LP->yrec[LP->recpos], LP->col + thrmax);
    }
  LP->yrec[LP->recpos] = yi;
  LP->xrec[LP->recpos] = xi;
  if (LP->recpos == LP->reclen - 1)
    LP->filled = True;
  if (LP->filled && !erasing)
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

static void
vermiculate_main (void)
{
  int had_instring = (instring != 0);
  int tick = 0;
  Bool halted = False, autopal = False, cleared;
  point = (unsigned char *) malloc (wid * hei);
  maininit ();
  palupdate (True);

  do
    {
      clearscreen ();
      {
	unsigned char thr;
	for (thr = 1; thr <= threads; thr++)
	  newonscreen (thr);
      }
      if (autopal)
	{
	  randpal ();
	  palupdate (False);
	}
      bordupdate ();
      gridupdate (False);
      cleared = False;
      do
	{
	  while (wasakeypressed ())
	    {
	      ch = readkey ();
	      switch (ch)
		{
		case 'M':
		  ch = readkey ();
		  switch (ch)
		    {
		    case 'A':
		    case 'N':
		      {
			unsigned char othreads = threads;
			if (ch == 'N')
			  threads = 0;
			do
			  {
			    ch = readkey ();
			    switch (ch)
			      {
			      case '1'...tmodes:
				thread[++threads - 1].tmode = ch - '0';
				break;
			      case 'R':
				thread[++threads - 1].tmode =
				  random1 (tmodes - '0') + 1;
				break;
			      }
			  }
			while (!(ch == '\15' || ch == '#'
				 || threads == thrmax));
			if (threads == 0)
			  threads = othreads;
			cleared = True;
		      }
		      break;
		    }
		  break;
		case 'C':
		  pickbank ();
		  if (bankt > 0)
		    {
		      ch = readkey ();
		      switch (ch)
			{
			case 'D':
			  ch = readkey ();
			  switch (ch)
			    {
			    case '1'...'9':
/* Careful!  The following macro needs to be at the beginning of any
block in which it's invoked, since it declares variables: */
#define forallinbank(LDP) linedata *LDP; int bankc; \
		for (bankc = 1;	\
		(LDP = &thread[bank[bankc - 1] - 1],	\
		bankc <= bankt); bankc++)
			      {
				forallinbank (L) L->slice = degs / (ch - '0');
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
			      char oldch = ch;
			      ch = readkey ();
			      {
				forallinbank (L)
				{
				  switch (ch)
				    {
				    case '0'...'9':
				      L->tslen++;
				      L->turnseq[L->tslen - 1] = ch - '0';
				      if (oldch == '-')
					L->turnseq[L->tslen - 1] *= -1;
				      if (bankc % 2 == 0)
					L->turnseq[L->tslen - 1] *= -1;
				      break;
				    }
				}
			      }
			    }
			  while (!(ch == '\15' || ch == '#'
				   || thread[bank[0] - 1].tslen == 50));
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
			    ch = readkey ();
			    {
			      forallinbank (L)
			      {
				switch (ch)
				  {
				  case '1'...tmodes:
				    L->tmode = ch - '0';
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
			  ch = readkey ();
			  {
			    forallinbank (L) L->orichar = ch;
			  }
			  break;
			case 'F':
			  {
			    banktype fbank;
			    arrcpy (fbank, bank);
			    {
			      int fbankt = bankt;
			      int bankc;
			      pickbank ();
			      for (bankc = 1; bankc <= fbankt; bankc++)
				{
				  linedata *L = &thread[fbank[bankc - 1] - 1];
				  if (ch == 'N')
				    L->prey = 0;
				  else
				    L->prey = bank[0 + (bankc - 1) % bankt];
				}
			    }
			  }
			  break;
			case 'L':
			  {
			    forallinbank (L) L->prey = bank[bankc % bankt];
			  }
			  break;
			case 'R':
			  ch = readkey ();
			  {
			    forallinbank (L) switch (ch)
			      {
			      case '1'...'9':
				L->circturn = 10 - (ch - '0');
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
		  boolop = ch;
		  pickbank ();
		  if (bankt > 0)
		    {
		      ch = readkey ();
		      {
			forallinbank (L)
			{
			  switch (ch)
			    {
			    case 'S':
			      bankmod (&L->selfbounce);
			      break;
			    case 'V':
			      bankmod (&L->vhfollow);
			      break;
			    case 'R':
			      bankmod (&L->realbounce);
			      break;
			    case 'L':
			      bankmod (&L->little);
			      cleared = True;
			      break;
			    case 'T':
			      bankmod (&L->tailfollow);
			      break;
			    case 'K':
			      bankmod (&L->killwalls);
			      break;
			    }
			}
		      }
		    }
		  break;
		case 'R':
		  if (bordcol == 1)
		    {
		      bordcol = 0;
		      bordupdate ();
		      bordcorn = (bordcorn + 1) % 4;
		      bordcol = 1;
		      bordupdate ();
		    }
		  break;
		case '\33':
		  halted = True;
		  break;
		case '1'...tmodes:
		  {
		    int c;
		    for (c = 1; c <= thrmax; c++)
		      thread[c - 1].tmode = ch - '0';
		  }
		  break;
		case '\40':
		  cleared = True;
		  break;
		case 'E':
		  erasing = !erasing;
		  break;
		case 'P':
		  randpal ();
		  palupdate (True);
		  break;
		case 'G':
		  {
		    char dimch = 'B';
		    Bool gridchanged = True;
		    if (gridden == 0)
		      gridden = ogd;
		    do
		      {
			int msize = 0;
			if (gridchanged)
			  {
			    clearscreen ();
			    gridupdate (True);
			  }
			ch = readkey ();
			gridchanged = True;
			switch (ch)
			  {
			  case '+':
			    msize = 1;
			    break;
			  case '-':
			    msize = -1;
			    break;
			  case ']':
			    if (gridden < 15)
			      gridden++;
			    break;
			  case '[':
			    if (gridden > 0)
			      gridden--;
			    break;
			  case 'O':
			    ogd = gridden;
			    gridden = 0;
			    break;
			  case 'S':
			    boxw = boxh;
			  case 'W':
			  case 'H':
			  case 'B':
			    dimch = ch;
			    break;
			  default:
			    gridchanged = False;
			  }
			if (dimch == 'W' || dimch == 'B')
			  boxw += msize;
			if (dimch == 'H' || dimch == 'B')
			  boxh += msize;
			if (boxw == 0)
			  boxw = 1;
			if (boxh == 0)
			  boxh = 1;
		      }
		    while (!(ch == '\15' || ch == '#' || ch == 'O'));
		    cleared = True;
		  }
		  break;
		case 'A':
		  autopal = !autopal;
		  break;
		case 'B':
		  bordcol = 1 - bordcol;
		  bordupdate ();
		  break;
		case '-':
		  speed -= SPEEDINC;
		  if (speed < 1)
		    speed = 1;
		  break;
		case '+':
		  speed += SPEEDINC;
		  if (speed > SPEEDMAX)
		    speed = SPEEDMAX;
		  break;
		case '/':
		  if (curviness > 5)
		    curviness -= 5;
		  break;
		case '*':
		  if (curviness < 50)
		    curviness += 5;
		  break;
		case ']':
		  if (threads < thrmax)
		    newonscreen (++threads);
		  break;
		case '[':
		  if (threads > 1)
		    {
		      linedata *L = &thread[threads - 1];
		      int lastpos = (L->filled) ? L->reclen - 1 : L->recpos;
		      int c;
		      for (c = 0; c <= lastpos; c++)
			sp (L->xrec[c], L->yrec[c], 0);
		      threads--;
		    }
		  break;
		}
	    }

#ifdef VERMICULATE_STANDALONE
	  {
	    XEvent xe;
	    while (XCheckWindowEvent
		   (mydpy, mywindow, StructureNotifyMask | ExposureMask, &xe))
	      switch (xe.type)
		{
		case ConfigureNotify:
		  wid = xe.xconfigure.width;
		  hei = xe.xconfigure.height;
		  free (point);
		  point = (unsigned char *) malloc (wid * hei);
		  cleared = True;
		  break;
		case Expose:
		  if (!cleared)
		    redraw (xe.xexpose.x,
			    xe.xexpose.y, xe.xexpose.width,
			    xe.xexpose.height);
		  break;
		}
	  }
#else
	  screenhack_handle_events (mydpy);
#endif /* VERMICULATE_STANDALONE */

	  if (!cleared)
	    {
	      Bool alltrap = True;
	      unsigned char thr;
	      for (thr = 1; thr <= threads; thr++)
		if (move (thr))
		  alltrap = False;
	      if (alltrap)	/* all threads are trapped */
		cleared = True;
	      if (speed != SPEEDMAX)
		waitabit ();
	    }

          if (tick++ > max_ticks && !had_instring)
            {
              tick = 0;
              instring = 0;
              maininit();
              cleared = True;
              autopal = False;
            }
	}
      while (!(halted || cleared));
    }
  while (!halted);
}

void
commonXinit (void)
{
  XSetWindowBackground (mydpy, mywindow,
			BlackPixel (mydpy, DefaultScreen (mydpy)));
  {
    XGetWindowAttributes (mydpy, mywindow, &xgwa);
    wid = xgwa.width;
    hei = xgwa.height;
    mycmap = xgwa.colormap;
  }
  {
    XGCValues mygcv;
    XGetGCValues (mydpy, XDefaultGC (mydpy, XDefaultScreen (mydpy)),
		  GCForeground, &mygcv);
    mygc = XCreateGC (mydpy, mywindow, GCForeground, &mygcv);
  }
}

#ifdef VERMICULATE_STANDALONE
/* Function Name: GetVRoot (slightly changed from the X Windows FAQ)
 * Description: Gets the root window, even if it's a virtual root
 * Arguments: the display and the screen
 * Returns: the root window for the client
 */
static Window
GetVRoot (Display * dpy, int scr)
{
  Window rootReturn, parentReturn, *children;
  unsigned int numChildren;
  Window root = RootWindow (dpy, scr);
  Atom __SWM_VROOT = None;
  int i;

  __SWM_VROOT = XInternAtom (dpy, "__SWM_VROOT", False);
  XQueryTree (dpy, root, &rootReturn, &parentReturn, &children, &numChildren);
  for (i = 0; i < numChildren; i++)
    {
      Atom actual_type;
      int actual_format;
      unsigned long int nitems, bytesafter;
      Window *newRoot = NULL;

      if (XGetWindowProperty (dpy, children[i], __SWM_VROOT, 0, 1,
			      False, XA_WINDOW, &actual_type, &actual_format,
			      &nitems, &bytesafter,
			      (unsigned char **) &newRoot) == Success
	  && newRoot)
	{
	  root = *newRoot;
	  break;
	}
    }

  XFree ((char *) children);
  return root;
}

int
main (int argc, char **argv)
{
  int argnum;
  if ((mydpy = XOpenDisplay (NULL)) == NULL)
    {
      fprintf (stderr, "%s: cannot connect to X server %s\n", argv[0],
	       XDisplayName (NULL));
      exit (1);
    }

  for (argnum = 1; argnum < argc; argnum++)
    {
      if (!strcmp (argv[argnum], "-geometry"))
	{
	  int x, y;
	  unsigned int uh, uw;
	  XParseGeometry (argv[++argnum], &x, &y, &uw, &uh);
	  hei = (int) uh;
	  wid = (int) uw;
	}
      else if (!strcmp (argv[argnum], "-instring"))
	instring = argv[++argnum];
      else if (!strcmp (argv[argnum], "-root"))
	use_root = True;
      else if (!strcmp (argv[argnum], "-speed"))
	speed = atoi (argv[++argnum]);
      else
	{
	  fprintf (stderr,
		   "\nvermiculate options are:"
		   "\n -speed NUMBER:  set speed, can be from 1 to %d."
		   "\n -root:  use root window."
		   "\n -instring STRING:  put STRING in kbd buffer."
		   "\n -geometry WIDTHxHEIGHT \n", SPEEDMAX);
	  exit (1);
	}
    }

  if (use_root)
    mywindow = GetVRoot (mydpy, DefaultScreen (mydpy));
  else
    mywindow = XCreateSimpleWindow (mydpy, DefaultRootWindow (mydpy), 0, 0,
				    wid, hei, 0, 0, BlackPixel (mydpy,
								DefaultScreen
								(mydpy)));
  XStoreName (mydpy, mywindow, "vermiculate");
  XMapWindow (mydpy, mywindow);
  commonXinit ();
  XSelectInput (mydpy, mywindow,
		KeyPressMask | ExposureMask | StructureNotifyMask);

#undef ya_rand_init
  ya_rand_init (0);

  vermiculate_main ();
  return 0;
}

#else

void
screenhack (Display * d, Window w)
{
  mydpy = d;
  mywindow = w;
  instring = get_string_resource ("instring", "Instring");
  max_ticks = get_integer_resource ("ticks", "Integer");
  {
    int temp = get_integer_resource ("speed", "Speed");
    if (temp != 0)
      speed = temp;
  }
  commonXinit ();
  mycolors[0].pixel = BlackPixel (mydpy, DefaultScreen (mydpy));
  vermiculate_main ();
}
#endif /* VERMICULATE_STANDALONE */
