/* -*- Mode: C; tab-width: 4 -*- */
/* dclock --- floating digital clock or message */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)dclock.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (C) 1995 by Michael Stembera <mrbig@fc.net>.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 07-May-1999: New "savers" added for y2k and second millennium countdowns.
 *              Tom Schmidt <tschmidt@micron.com>
 * 04-Dec-1998: New "savers" added for hiv, veg, and lab.
 *              hiv one due to Kenneth Stailey <kstailey@disclosure.com>
 * 10-Aug-1998: Population Explosion and Tropical Forest Countdown stuff
 *              I tried to get precise numbers but they may be off a few
 *              percent.  Whether or not, its still pretty scary IMHO.
 *              Although I am a US citizen... I have the default for area in
 *              metric.  David Bagley <bagleyd@tux.org>
 * 10-May-1997: Compatible with xscreensaver
 * 29-Aug-1995: Written.
 */

/*-
 *  Some of my calculations laid bare...  (I have a little problem with
 *  the consistency of the numbers I got at the Bronx Zoo but proportions
 *  were figured to be 160.70344 people a minute increase not 180 and
 *  35.303144 hectares (87.198766 acres) a minute decrease not 247 (100 acres).
 *  So I am going with these more conservative numbers.)
 *
 *  Time 0 is 0:00:00 1 Jan 1970 at least according to hard core UNIX fans
 *  Minutes from 1  Jan 1970 to 21 Jun 1985:  8137440
 *  Minutes from 21 Jun 1985 to 12 Jul 1988:  6867360
 *                                    Total: 15004800
 *
 *  Population Explosion Saver (popex)
 *  3,535,369,000 people (figured by extrapolation) 1 Jan 1970
 *  4,843,083,596 people 21 Jun 1985 (Opening of Wild Asia in the Bronx Zoo)
 *  5,946,692,000 people 12 Jul 1998 (David Bagley visits zoo ;) )
 *  180 people a minute increase in global population (I heard 170 also)
 *  260,000 people a day increase in global population
 *
 *  Tropical Forest Countdown Saver (forest)
 *  1,184,193,000 hectares (figured by extrapolation) 1 Jan 1970
 *    (2,924,959,000 acres)
 *  896,916,700 hectares 21 Jun 1985 (Opening of Wild Asia in the Bronx Zoo)
 *    (2,215,384,320 acres)
 *  654,477,300 hectares 12 Jul 1998 (David Bagley visits zoo ;) )
 *    (1,616,559,000 acres)
 *  247 hectares a minute lost forever (1 hectare = 2.47 acres)
 *
 *  HIV Infection Counter Saver (hiv) (stats from http://www.unaids.org/)
 *  33,600,000 31 Dec 1999 living w/HIV
 *  16,300,000 31 Dec 1999 dead
 *  49,900,000 31 Dec 1999 total infections
 *   5,600,000 new infections in 1999
 *  10.6545 infections/min
 *  -118,195,407 virtual cases (figured by extrapolation) 1 Jan 1970
 *   (this is a result of applying linear tracking to a non-linear event)
 *
 *  Animal Research Counter Saver (lab)
 *  Approximately 17-22 million animals are used in research each year.
 *  OK so assume 17,000,000 / 525960 = 32.32184957 per minute
 *  http://www.fcs.uga.edu/~mhulsey/GDB.html
 *
 *  Animal Consumation Counter Saver (veg)
 *  Approximately 5 billion are consumed for food annually.
 *  OK 5,000,000,000 / 525960 = 9506.426344209 per minute
 *  http://www.fcs.uga.edu/~mhulsey/GDB.html
 */

#ifdef STANDALONE
#define MODE_dclock
#define PROGCLASS "Dclock"
#define HACK_INIT init_dclock
#define HACK_DRAW draw_dclock
#define dclock_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*cycles: 10000 \n" \
 "*ncolors: 64 \n"
#define BRIGHT_COLORS
#define UNIFORM_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#include "mode.h"
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "util.h"
#endif /* STANDALONE */
#include "iostuff.h"

#ifdef MODE_dclock

#ifndef METRIC
#define METRIC 1
#endif

#if METRIC
#define AREA_MIN 35.303144
#define AREA_TIME_START 1184193000.0
#else
#define AREA_MIN 87.198766
#define AREA_TIME_START 2924959000.0
#endif

#define PEOPLE_MIN 160.70344
#define PEOPLE_TIME_START 3535369000.0
#define HIV_MIN 10.6545
#define HIV_TIME_START -118195407.0
#define LAB_MIN 32.32184957
#define LAB_TIME_START 0
#define VEG_MIN 9506.426344209
#define VEG_TIME_START 0
/* epoch time at midnight 1 January 2000 UTC */
#define Y2K_TIME_START (((30 * 365) + 7) * 24 * 60 * 60)
#define Y2001_TIME_START (Y2K_TIME_START + 366 * 24 * 60 * 60)

#define LED_LEAN 0.2
#define LED_XS 30.0
#define LED_YS 45.0
#define LED_WIDTH (0.15 * LED_YS)
#define LED_INC (LED_LEAN * LED_XS) /* LEAN and INC do not have to be linked */

#define DEF_LED    "False"
#define DEF_POPEX  "False"
#define DEF_FOREST "False"
#define DEF_HIV    "False"
#define DEF_LAB    "False"
#define DEF_VEG    "False"
#define DEF_Y2K    "False"
#define DEF_Y2001  "False"

/*- If you remember your Electronics course...
        a
        _
    f / / b
      -g
  e /_/ c
    d
 */

#define MAX_LEDS 7
static unsigned char digits[][MAX_LEDS]=
{
/* a  b  c  d  e  f  g */
  {1, 1, 1, 1, 1, 1, 0},  /* 0 */
  {0, 1, 1, 0, 0, 0, 0},  /* 1 */
  {1, 1, 0, 1, 1, 0, 1},  /* 2 */
  {1, 1, 1, 1, 0, 0, 1},  /* 3 */
  {0, 1, 1, 0, 0, 1, 1},  /* 4 */
  {1, 0, 1, 1, 0, 1, 1},  /* 5 */
  {1, 0, 1, 1, 1, 1, 1},  /* 6 */
  {1, 1, 1, 0, 0, 0, 0},  /* 7 */
  {1, 1, 1, 1, 1, 1, 1},  /* 8 */
  {1, 1, 1, 1, 0, 1, 1}  /* 9 */
#if 0
  , /* Completeness, we must have completeness */
  {1, 1, 1, 0, 1, 1, 1},  /* A */
  {0, 0, 1, 1, 1, 1, 1},  /* b */
  {1, 0, 0, 1, 1, 1, 0},  /* C */
  {0, 1, 1, 1, 1, 0, 1},  /* d */
  {1, 0, 0, 1, 1, 1, 1},  /* E */
  {1, 0, 0, 0, 1, 1, 1}   /* F */
#define MAX_DIGITS 16
#else
#define MAX_DIGITS 10
#endif
};

static Bool led;

/* Create an virtual parallelogram, normal rectangle parameters plus "lean":
   the amount the start of a will be shifted to the right of the start of d.
 */

static XPoint parallelogramUnit[4] =
{
	{1, 0},
	{2, 0},
	{-1, 1},
	{-2, 0}
};

static Bool popex;
static Bool forest;
static Bool hiv;
static Bool lab;
static Bool veg;
static Bool y2k;
static Bool millennium;

static XrmOptionDescRec opts[] =
{
	{(char *) "-led", (char *) ".dclock.led", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+led", (char *) ".dclock.led", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-popex", (char *) ".dclock.popex", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+popex", (char *) ".dclock.popex", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-forest", (char *) ".dclock.forest", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+forest", (char *) ".dclock.forest", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-hiv", (char *) ".dclock.hiv", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+hiv", (char *) ".dclock.hiv", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-lab", (char *) ".dclock.lab", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+lab", (char *) ".dclock.lab", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-veg", (char *) ".dclock.veg", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+veg", (char *) ".dclock.veg", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-y2k", (char *) ".dclock.y2k", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+y2k", (char *) ".dclock.y2k", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-millennium", (char *) ".dclock.millennium", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+millennium", (char *) ".dclock.millennium", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(void *) & led, (char *) "led", (char *) "LED", (char *) DEF_LED, t_Bool},
	{(void *) & popex, (char *) "popex", (char *) "PopEx", (char *) DEF_POPEX, t_Bool},
	{(void *) & forest, (char *) "forest", (char *) "Forest", (char *) DEF_FOREST, t_Bool},
	{(void *) & hiv, (char *) "hiv", (char *) "Hiv", (char *) DEF_HIV, t_Bool},
	{(void *) & lab, (char *) "lab", (char *) "Lab", (char *) DEF_LAB, t_Bool},
	{(void *) & veg, (char *) "veg", (char *) "Veg", (char *) DEF_VEG, t_Bool},
	{(void *) & y2k, (char *) "y2k", (char *) "Y2K", (char *) DEF_Y2K, t_Bool},
	{(void *) & millennium, (char *) "millennium", (char *) "Millennium", (char *) DEF_Y2001, t_Bool}
};
static OptionStruct desc[] =
{
	{(char *) "-/+led", (char *) "turn on/off Light Emitting Diode seven segment display"},
	{(char *) "-/+popex", (char *) "turn on/off population explosion counter"},
	{(char *) "-/+forest", (char *) "turn on/off tropical forest destruction counter"},
	{(char *) "-/+hiv", (char *) "turn on/off HIV infection counter"},
	{(char *) "-/+lab", (char *) "turn on/off Animal Research counter"},
	{(char *) "-/+veg", (char *) "turn on/off Animal Consumation counter"},
	{(char *) "-/+y2k", (char *) "turn on/off Year 2000 countdown"},
	{(char *) "-/+millennium", (char *) "turn on/off 3rd Millennium (1 January 2001) countdown"},
};

ModeSpecOpt dclock_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   dclock_description =
{"dclock", "init_dclock", "draw_dclock", "release_dclock",
 "refresh_dclock", "init_dclock", (char *) NULL, &dclock_opts,
 10000, 1, 10000, 1, 64, 0.3, "",
 "Shows a floating digital clock or message", 0, NULL};

#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <time.h>

/* language depended resources */
#if defined FR
#include "dclock-msg-fr.h"
#elif defined NL
#include "dclock-msg-nl.h"
#elif defined JA
#include "dclock-msg-ja.h"
#else
#include "dclock-msg-en.h"
#endif

#ifdef USE_MB
static XFontSet mode_font = None;
static int font_height(XFontSet f)
{
  XRectangle ink, log;
  if (f == None) {
    return 8;
  } else {
      XmbTextExtents(mode_font, "My", strlen("My"), &ink, &log);
      return log.height;
  }
}
#define DELTA 0.2
extern XFontSet getFontSet(Display * display);
#else
static XFontStruct *mode_font = None;
#define font_height(f) (f->ascent + f->descent)
#define DELTA 0
extern XFontStruct *getFont(Display * display);
#endif

#ifdef USE_MB
#define free_font(d) if (mode_font!=None){XFreeFontSet(d,mode_font); \
mode_font = None;}
#else
#define free_font(d) if (mode_font!=None){XFreeFont(d,mode_font); \
mode_font = None;}
#endif

#define STRSIZE 50

typedef struct {
	int         color;
	short       height, width;
	char       *str, str1[STRSIZE], str2[STRSIZE], str1old[STRSIZE], str2old[STRSIZE];
	char       *str1pta, *str2pta, *str1ptb, *str2ptb;
	time_t      timenew, timeold;
	int         tzoffset;
	short       maxx, maxy, clockx, clocky;
	short       text_height, text_width, text_width1, text_width2;
	short       text_start1, text_start2;
	short       text_ascent, text_descent;
	short       hour;
	short       dx, dy;
	int         done;
	int         pixw, pixh;
	Pixmap      pixmap;
	GC          fgGC, bgGC;
	Bool        popex, forest, hiv, lab, veg, y2k, millennium, led;
	XPoint      parallelogram[4];
} dclockstruct;

static dclockstruct *dclocks = (dclockstruct *) NULL;


static unsigned long
timeAtLastNewYear(long timeNow)
{
  struct tm *t;

  t = localtime((const time_t *) &timeNow);
  return (unsigned long)(t->tm_year);
}

#ifndef HAVE_SNPRINTF
static double logbase;
#define BASE 10.0
#define GROUP 3
#endif

static void
convert(double x, char *string)
{
#ifdef HAVE_SNPRINTF
/* Also old C compiler can not accept this syntax, but this syntax awares
   locale.  Known to work with gcc-2.95.2 and glibc-2.1.3. */
	(void) snprintf(string, STRSIZE, "%'.0f", x);
#else

	int         i, j, k = 0;
	int         place = (int) (log(x) / logbase);
	double      divisor = 1.0;

	for (i = 0; i < place; i++)
		divisor *= BASE;

	for (i = place; i >= 0; i--) {
		j = (int) (x / divisor);
		string[k++] = (char) j + '0';
		x -= j * divisor;
		divisor /= BASE;
		if ((i > 0) && (i % GROUP == 0)) {
			string[k++] = ',';
		}
	}
	string[k] = '\0';
#endif
}

static void
dayhrminsec(long timeCount, int tzoffset, char *string)
{
	int days, hours, minutes, secs;
	int bufsize, i;

	timeCount = ABS(timeCount);
	days = (int) (timeCount / 86400);
	hours = (int) ((timeCount / 3600) % 24);
	minutes = (int) ((timeCount / 60) % 60);
	secs = (int) (timeCount % 60);

	/* snprintf would make this easier but its not always available */
	bufsize = 16 + strlen((days==1) ? DAY : DAYS);
	if (bufsize >= STRSIZE)
          return;
	(void) sprintf(string, "%d %s", days, (days==1) ? DAY : DAYS);

	i = strlen(string);
	bufsize = 4 + strlen((hours==1) ? HOUR : HOURS);
	if (i + bufsize >= STRSIZE)
          return;
	(void) sprintf(&string[i], " %d %s", hours, (hours==1) ? HOUR : HOURS);

	i = strlen(string);
	bufsize = 4 + strlen((minutes==1) ? MINUTE : MINUTES);
	if (i + bufsize >= STRSIZE)
          return;
	(void) sprintf(&string[i], " %d %s", minutes, (minutes==1) ? MINUTE : MINUTES);

	i = strlen(string);
	bufsize += 4 + strlen((secs==1) ? SECOND : SECONDS);
	if (i + bufsize >= STRSIZE)
          return;
	(void) sprintf(&string[i], " %d %s", secs, (secs==1) ? SECOND : SECONDS);

	if (!tzoffset) {
		i = strlen(string);
		bufsize += 6;  /* strlen(" (UTC)"); */
        	if (i + bufsize >= STRSIZE)
          		return;
		(void) strcat(string, " (UTC)");
	}
}

static void
drawaled(ModeInfo * mi, int startx, int starty, int aled)
{
	Display    *display = MI_DISPLAY(mi);
	dclockstruct *dp = &dclocks[MI_SCREEN(mi)];
 	int x_1, y_1, x_2, y_2;

	int offset = (int) LED_WIDTH;
	int offset2 = (int) (LED_WIDTH / 2.0);
	int leanoffset = (int) (offset2 * LED_LEAN);

	switch (aled) {
		case 0: /* a */
			x_1 = startx + dp->parallelogram[0].x;
			y_1 = starty + dp->parallelogram[0].y;
			x_2 = x_1 + dp->parallelogram[1].x - offset;
			y_2 = y_1 + dp->parallelogram[1].y;
			x_1 = x_1 + offset;
			XDrawLine(display, dp->pixmap, dp->fgGC,
				x_1, y_1, x_2, y_2);
			break;
		case 1: /* b */
			x_1 = startx + dp->parallelogram[0].x + dp->parallelogram[1].x;
			y_1 = starty + dp->parallelogram[0].y + dp->parallelogram[1].y;
			x_2 = x_1 + dp->parallelogram[2].x / 2 + leanoffset;
			y_2 = y_1 + dp->parallelogram[2].y / 2 - offset2;
			x_1 = x_1 - leanoffset;
			y_1 = y_1 + offset2;
			XDrawLine(display, dp->pixmap, dp->fgGC,
				x_1, y_1, x_2, y_2);
			break;
		case 2: /* c */
			x_1 = startx + dp->parallelogram[0].x + dp->parallelogram[1].x + dp->parallelogram[2].x;
			y_1 = starty + dp->parallelogram[0].y + dp->parallelogram[1].y + dp->parallelogram[2].y;
			x_2 = x_1 - dp->parallelogram[2].x / 2 - leanoffset;
			y_2 = y_1 - dp->parallelogram[2].y / 2 + offset2;
			x_1 = x_1 + leanoffset;
			y_1 = y_1 - offset2;
			XDrawLine(display, dp->pixmap, dp->fgGC,
				x_1, y_1, x_2, y_2);
			break;
		case 3: /* d */
			x_1 = startx + dp->parallelogram[0].x + dp->parallelogram[2].x;
			y_1 = starty + dp->parallelogram[0].y + dp->parallelogram[2].y;
			x_2 = x_1 + dp->parallelogram[1].x - offset;
			y_2 = y_1 + dp->parallelogram[1].y;
			x_1 = x_1 + offset;
			XDrawLine(display, dp->pixmap, dp->fgGC,
				x_1, y_1, x_2, y_2);
			break;
		case 4: /* e */
			x_1 = startx + dp->parallelogram[0].x + dp->parallelogram[2].x;
			y_1 = starty + dp->parallelogram[0].y + dp->parallelogram[2].y;
			x_2 = x_1 - dp->parallelogram[2].x / 2 - leanoffset;
			y_2 = y_1 - dp->parallelogram[2].y / 2 + offset2;
			x_1 = x_1 + leanoffset;
			y_1 = y_1 - offset2;
			XDrawLine(display, dp->pixmap, dp->fgGC,
				x_1, y_1, x_2, y_2);
			break;
		case 5: /* f */
			x_1 = startx + dp->parallelogram[0].x;
			y_1 = starty + dp->parallelogram[0].y;
			x_2 = x_1 + dp->parallelogram[2].x / 2 + leanoffset;
			y_2 = y_1 + dp->parallelogram[2].y / 2 - offset2;
			x_1 = x_1 - leanoffset;
			y_1 = y_1 + offset2;
			XDrawLine(display, dp->pixmap, dp->fgGC,
				x_1, y_1, x_2, y_2);
			break;
		case 6: /* g */
			x_1 = startx + dp->parallelogram[0].x + dp->parallelogram[2].x / 2;
			y_1 = starty + dp->parallelogram[0].y + dp->parallelogram[2].y / 2;
			x_2 = x_1 + dp->parallelogram[1].x - offset;
			y_2 = y_1 + dp->parallelogram[1].y;
			x_1 = x_1 + offset;
			XDrawLine(display, dp->pixmap, dp->fgGC,
				x_1, y_1, x_2, y_2);

	}
}

static void
drawacolon(ModeInfo * mi, int startx, int starty)
{
	Display    *display = MI_DISPLAY(mi);
	dclockstruct *dp = &dclocks[MI_SCREEN(mi)];
 	int x_1, y_1, x_2, y_2;

	int offset2 = (int) (LED_WIDTH / 2.0);
	int leanoffset = (int) (offset2 * LED_LEAN);

	x_1 = startx + dp->parallelogram[0].x +
		(int) (dp->parallelogram[2].x / 2 - 2.0 * leanoffset);
	y_1 = starty + dp->parallelogram[0].y + dp->parallelogram[2].y / 2 +
		(int) (2.0 * offset2);
	x_2 = x_1 - (int) (2.0 * leanoffset);
	y_2 = y_1 + (int) (2.0 * offset2);
	XDrawLine(display, dp->pixmap, dp->fgGC, x_1, y_1, x_2, y_2);
	x_1 = startx + dp->parallelogram[0].x +
		dp->parallelogram[2].x / 2 + (int) (2.0 * leanoffset);
	y_1 = starty + dp->parallelogram[0].y + dp->parallelogram[2].y / 2 -
		(int) (2.0 * offset2);
	x_2 = x_1 + (int) (2.0 * leanoffset);
	y_2 = y_1 - (int) (2.0 * offset2);
	XDrawLine(display, dp->pixmap, dp->fgGC, x_1, y_1, x_2, y_2);
}

static void
drawanumber(ModeInfo * mi, int startx, int starty, int digit)
{
	int aled;

	for (aled = 0; aled < MAX_LEDS; aled++) {
		if (digits[digit][aled])
			drawaled(mi, startx, starty, aled);
	}
}

static void
free_dclock(Display *display, dclockstruct *dp)
{
	if (dp->fgGC != None) {
		XFreeGC(display, dp->fgGC);
		dp->fgGC = None;
	}
	if (dp->bgGC) {
		XFreeGC(display, dp->bgGC);
		dp->bgGC = None;
	}
	if (dp->pixmap) {
		XFreePixmap(display, dp->pixmap);
		dp->pixmap = None;
	}
}

static void
drawDclock(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	dclockstruct *dp = &dclocks[MI_SCREEN(mi)];
	short       xold, yold;

	if (dp->led) {
		dp->timeold = dp->timenew = time((time_t *) NULL);
		dp->str = ctime(&dp->timeold);
	} else if (!dp->popex && !dp->forest && !dp->hiv &&
	    !dp->lab && !dp->veg && !dp->y2k && !dp->millennium) {
		if (dp->timeold != (dp->timenew = time((time_t *) NULL))) {
			/* only parse if time has changed */
			dp->timeold = dp->timenew;

			if (!dp->popex && !dp->forest && !dp->hiv && !dp->lab &&
			    !dp->veg && !dp->y2k && !dp->millennium) {
				(void) strftime(dp->str1pta, STRSIZE, "%I:%M:%S %p", localtime(&(dp->timeold)));
			}
			(void) strftime(dp->str2pta, STRSIZE, "%a %b %d %Y", localtime(&(dp->timeold)));
		}
	} else {
		long timeNow, timeLocal;
		timeNow = seconds();
		timeLocal = timeNow - dp->tzoffset;

		if (dp->popex) {
			convert(PEOPLE_TIME_START + (PEOPLE_MIN / 60.0) * timeNow, dp->str2);
			(void) strncat(dp->str2, PEOPLE_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->forest) {
			convert(AREA_TIME_START - (AREA_MIN / 60.0) * timeNow, dp->str2);
			(void) strncat(dp->str2, TROPICAL_STRING, STRSIZE-strlen(dp->str2));
			(void) strncat(dp->str2, AREA_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->hiv) {
			convert(HIV_TIME_START + (HIV_MIN / 60.0) * timeNow, dp->str2);
			(void) strncat(dp->str2, CASES_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->lab) {
			convert((LAB_MIN / 60.0) * (timeNow - timeAtLastNewYear(timeNow)),
				dp->str2);
			(void) strncat(dp->str2, YEAR_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->veg) {
			convert((VEG_MIN / 60.0) * (timeNow - timeAtLastNewYear(timeNow)),
				dp->str2);
			(void) strncat(dp->str2, YEAR_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->y2k) {
			if (Y2K_TIME_START >= timeLocal)
				dp->str1pta = (char *) Y2K_STRING;
			else
				dp->str1pta = (char *) POST_Y2K_STRING;
			dp->str1ptb = dp->str1pta;
			dayhrminsec(Y2K_TIME_START - timeLocal, dp->tzoffset, dp->str2);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->millennium) {
			if (Y2001_TIME_START >= timeLocal)
				dp->str1pta = (char *) Y2001_STRING;
			else
				dp->str1pta = (char *) POST_Y2001_STRING;
			dp->str1ptb = dp->str1pta;
			dayhrminsec(Y2001_TIME_START - timeLocal, dp->tzoffset, dp->str2);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		}
	}
	/* Recalculate string width since it can change */
	xold = dp->clockx;
	yold = dp->clocky;
	if (!dp->led) {
#ifdef USE_MB
		{
			XRectangle ink, logical;

			XmbTextExtents(mode_font, dp->str1pta, strlen(dp->str1pta), &ink, &logical);
			dp->text_width1 = logical.width;
			XmbTextExtents(mode_font, dp->str2pta, strlen(dp->str2pta), &ink, &logical);
			dp->text_width2 = logical.width;
		}
#else
		dp->text_width1 = XTextWidth(mode_font, dp->str1pta, strlen(dp->str1pta));
		dp->text_width2 = XTextWidth(mode_font, dp->str2pta, strlen(dp->str2pta));
#endif

		if (dp->text_width1 > dp->text_width2) {
			dp->text_width = dp->text_width1;
			dp->text_start1 = 0;
			dp->text_start2 = (dp->text_width - dp->text_width2) / 2;
		} else {
			dp->text_width = dp->text_width2;
			dp->text_start1 = (dp->text_width - dp->text_width1) / 2;
			dp->text_start2 = 0;
		}
	}
	dp->width = MI_WIDTH(mi);
	dp->height = MI_HEIGHT(mi);
	dp->maxx = dp->width - dp->text_width;
	dp->maxy = dp->height - dp->text_height - dp->text_descent;
	dp->clockx += dp->dx;
	dp->clocky += dp->dy;
	if (dp->maxx < dp->text_start1) {
		if (dp->clockx < dp->maxx + dp->text_start1 ||
		    dp->clockx > dp->text_start1) {
			dp->dx = -dp->dx;
			dp->clockx += dp->dx;
		}
	} else if (dp->maxx > dp->text_start1) {
		if (dp->clockx >= dp->maxx + dp->text_start1 ||
		    dp->clockx <= dp->text_start1) {
			dp->dx = -dp->dx;
			dp->clockx += dp->dx;
		}
	}
	if (dp->maxy < dp->text_ascent) {
		if (dp->clocky > dp->text_ascent || dp->clocky < dp->maxy) {
			dp->dy = -dp->dy;
			dp->clocky += dp->dy;
		}
	} else if (dp->maxy > dp->text_ascent) {
		if (dp->clocky > dp->maxy || dp->clocky < dp->text_ascent) {
			dp->dy = -dp->dy;
			dp->clocky += dp->dy;
		}
	}
	if (dp->pixw != dp->text_width ||
	    dp->pixh != (1 + !dp->led) * dp->text_height) {
		XGCValues   gcv;

		if (dp->pixmap)
			MI_CLEARWINDOWCOLORMAPFAST(mi, gc, MI_BLACK_PIXEL(mi));
		free_dclock(display, dp);
		dp->pixw = dp->text_width;
		if (dp->led)
			dp->pixh = dp->text_height;
		else
			dp->pixh = (2 + DELTA) * dp->text_height;
		if ((dp->pixmap =
			XCreatePixmap(display, window, dp->pixw, dp->pixh, 1)) == None) {
			free_dclock(display, dp);
			dp->pixw = 0;
			dp->pixh = 0;
			return;
		}
#ifndef USE_MB
		gcv.font = mode_font->fid;
#endif
		gcv.background = 0;
		gcv.foreground = 1;
		gcv.graphics_exposures = False;
		if ((dp->fgGC = XCreateGC(display, dp->pixmap,
			GCForeground | GCBackground | GCGraphicsExposures
#ifndef USE_MB
			| GCFont
#endif
			, &gcv)) == None) {
			free_dclock(display, dp);
			dp->pixw = 0;
			dp->pixh = 0;
			return;
		}
		gcv.foreground = 0;
		if ((dp->bgGC = XCreateGC(display, dp->pixmap,
			GCForeground | GCBackground | GCGraphicsExposures
#ifndef USE_MB
			| GCFont
#endif
			, &gcv)) == None) {
			free_dclock(display, dp);
			dp->pixw = 0;
			dp->pixh = 0;
			return;
		}
		XSetLineAttributes(display, dp->fgGC,
		  (unsigned int) (LED_WIDTH),
		  LineSolid, CapButt, JoinMiter);
	}
	XFillRectangle(display, dp->pixmap, dp->bgGC, 0, 0, dp->pixw, dp->pixh);

	if (dp->led) {
		int startx = (int) (LED_WIDTH / 2.0);
		int starty = (int) (LED_WIDTH / 2.0);

		drawanumber(mi, startx, starty, dp->str[11] - '0');
		startx += (int) (LED_XS + LED_WIDTH + LED_INC);
		drawanumber(mi, startx, starty, dp->str[12] - '0');
		startx += (int) (LED_XS + LED_WIDTH + LED_INC);
		drawacolon(mi, startx, starty);
		startx += (int) (LED_WIDTH + LED_INC);
		drawanumber(mi, startx, starty, dp->str[14] - '0');
		startx += (int) (LED_XS + LED_WIDTH + LED_INC);
		drawanumber(mi, startx, starty, dp->str[15] - '0');
		startx += (int) (LED_XS + LED_WIDTH + LED_INC);
		drawacolon(mi, startx, starty);
		startx += (int) (LED_WIDTH + LED_INC);
		drawanumber(mi, startx, starty, dp->str[17] - '0');
		startx += (int) (LED_XS + LED_WIDTH + LED_INC);
		drawanumber(mi, startx, starty, dp->str[18] - '0');
	} else {
#ifdef USE_MB
		(void) XmbDrawString(display, dp->pixmap, mode_font, dp->fgGC,
			dp->text_start1, dp->text_ascent,
			dp->str1pta, strlen(dp->str1pta));
		(void) XmbDrawString(display, dp->pixmap, mode_font, dp->fgGC,
			dp->text_start2, dp->text_ascent + dp->text_height,
			dp->str2pta, strlen(dp->str2pta));
#else
		(void) XDrawString(display, dp->pixmap, dp->fgGC,
			dp->text_start1, dp->text_ascent,
			dp->str1pta, strlen(dp->str1pta));
		(void) XDrawString(display, dp->pixmap, dp->fgGC,
			dp->text_start2, dp->text_ascent + dp->text_height,
			dp->str2pta, strlen(dp->str2pta));
#endif
	}

	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	/* This could leave screen dust on the screen if the width changes
	   But that only happens once a day...
	   ... this is solved by the ClearWindow above
	 */
	ERASE_IMAGE(display, window, gc,
	    (dp->clockx - dp->text_start1), (dp->clocky - dp->text_ascent),
		    (xold - dp->text_start1), (yold - dp->text_ascent),
		    dp->pixw, dp->pixh);
	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, dp->color));
	else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	XCopyPlane(display, dp->pixmap, window, gc,
		   0, 0, dp->text_width, (2 + DELTA) * dp->text_height,
		dp->clockx - dp->text_start1, dp->clocky - dp->text_ascent,
		   1L);
}

void
release_dclock(ModeInfo * mi)
{
	if (dclocks != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_dclock(MI_DISPLAY(mi), &dclocks[screen]);
		free(dclocks);
		dclocks = (dclockstruct *) NULL;
	}
	free_font(MI_DISPLAY(mi));
}

#if defined(HAVE_TZSET) && !(defined(BSD) && BSD >= 199306) && !(defined(__CYGWIN__))
extern long timezone;
#endif

void
init_dclock(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	dclockstruct *dp;
	long timeNow, timeLocal;

	if (dclocks == NULL) {
#ifndef HAVE_SNPRINTF
		logbase = log(BASE);
#endif
		if ((dclocks = (dclockstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (dclockstruct))) == NULL)
			return;
	}
	dp = &dclocks[MI_SCREEN(mi)];

	dp->width = MI_WIDTH(mi);
	dp->height = MI_HEIGHT(mi);

	MI_CLEARWINDOW(mi);

	dp->led = False;
	dp->popex = False;
	dp->forest = False;
	dp->hiv = False;
	dp->lab = False;
	dp->veg = False;
	dp->y2k = False;
	dp->millennium = False;
#if defined(MODE_dclock_y2k) && defined(MODE_dclock_millennium)
#define NUM_DCLOCK_MODES 9
#endif
#if defined(MODE_dclock_y2k) && !defined(MODE_dclock_millennium)
#define NUM_DCLOCK_MODES 8
#endif
#if !defined(MODE_dclock_y2k) && defined(MODE_dclock_millennium)
#define NUM_DCLOCK_MODES 8
#endif
#if !defined(MODE_dclock_y2k) && !defined(MODE_dclock_millennium)
#define NUM_DCLOCK_MODES 7
#endif
	if (MI_IS_FULLRANDOM(mi)) {
		switch (NRAND(NUM_DCLOCK_MODES)) {
			case 0:
				break;
			case 1:
				dp->led = True;
				break;
			case 2:
				dp->popex = True;
				break;
			case 3:
				dp->forest = True;
				break;
			case 4:
				dp->hiv = True;
				break;
			case 5:
				dp->lab = True;
				break;
			case 6:
				dp->veg = True;
				break;
#if defined(MODE_dclock_y2k) || defined(MODE_dclock_millennium)
			case 7:
#ifdef MODE_dclock_y2k
				dp->y2k = True;
#else
				dp->millennium = True;
#endif
				break;
#if defined(MODE_dclock_y2k) && defined(MODE_dclock_millennium)
			case 8:
				dp->millennium = True;
				break;
#endif
#endif
			default:
				break;
		}
	} else { /* first come, first served */
		dp->led = led;
		dp->popex = popex;
		dp->forest = forest;
		dp->hiv = hiv;
		dp->lab = lab;
		dp->veg = veg;
		dp->y2k = y2k;
		dp->millennium = millennium;
	}

	if (mode_font == None) {
#ifdef USE_MB
		mode_font = getFontSet(display);
#else
		mode_font = getFont(display);
#endif
		if (mode_font == None) {
			release_dclock(mi);
			return;
		}
	}
	if (!dp->done) {
		dp->done = 1;
#ifndef USE_MB
		if (mode_font != None)
			XSetFont(display, MI_GC(mi), mode_font->fid);
#endif
	}
	/* (void)time(&dp->timenew); */
#if defined(HAVE_TZSET) && (!defined(HAVE_TIMELOCAL) || (defined(BSD) && BSD >= 199306))
	(void) tzset();
#endif
	dp->timeold = dp->timenew = time((time_t *) NULL);
#if defined(HAVE_TIMELOCAL) && !(defined(BSD) && BSD >= 199306)
	dp->tzoffset = mktime(localtime(&dp->timeold)) -
		mktime(gmtime(&dp->timeold));
#else
#if defined(HAVE_TZSET)
        dp->tzoffset = (int)timezone;
#else
	dp->tzoffset = 0;
#endif
#endif
	if (dp->tzoffset > 86400 || dp->tzoffset < -86400)
		dp->tzoffset = 0;
	dp->str = ctime(&dp->timeold);
	dp->dx = (LRAND() & 1) ? 1 : -1;
	dp->dy = (LRAND() & 1) ? 1 : -1;

	timeNow = seconds();
	timeLocal = timeNow - dp->tzoffset;
	if (dp->led) {
		int i;

		dp->text_descent = 0;
		dp->text_ascent = 0;
		for (i = 0; i < 4; i++) {
			if (parallelogramUnit[i].x == 1)
				dp->parallelogram[i].x = (short) (LED_XS * LED_LEAN);
			else if (parallelogramUnit[i].x == 2)
				dp->parallelogram[i].x = (short) LED_XS;
			else if (parallelogramUnit[i].x == -1)
				dp->parallelogram[i].x = (short) (-LED_XS * LED_LEAN);
			else if (parallelogramUnit[i].x == -2)
				dp->parallelogram[i].x = (short) (-LED_XS);
			else
				dp->parallelogram[i].x = 0;
			dp->parallelogram[i].y = (short) (LED_YS * parallelogramUnit[i].y);
		}

		dp->parallelogram[0].x = (short) ((LED_XS * LED_LEAN) + LED_INC);
		dp->parallelogram[0].y = (short) LED_INC;
		dp->text_width = (short) (6 * (LED_XS + LED_WIDTH + LED_INC) +
		  2 * (LED_WIDTH + LED_INC) + LED_XS * LED_LEAN - LED_INC);
		dp->text_height = (short) (LED_YS + LED_WIDTH + LED_INC);
		dp->maxy = dp->height - dp->text_height;
		if (dp->maxy == 0)
			dp->clocky = 0;
		else if (dp->maxy < 0)
			dp->clocky = -NRAND(-dp->maxy);
		else
			dp->clocky = NRAND(dp->maxy);
	} else {
#ifdef USE_MB
		dp->text_descent = 0;
		dp->text_ascent = font_height(mode_font);
#else
		dp->text_descent = mode_font->descent;
		dp->text_ascent = mode_font->ascent;
#endif
		if (dp->popex) {
			dp->str1pta = (char *) POPEX_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->forest) {
			dp->str1pta = (char *) FOREST_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->hiv) {
			dp->str1pta = (char *) HIV_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->lab) {
			dp->str1pta = (char *) LAB_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->veg) {
			dp->str1pta = (char *) VEG_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->y2k) {
			if (Y2K_TIME_START >= timeLocal)
				dp->str1pta = (char *) Y2K_STRING;
			else
				dp->str1pta = (char *) POST_Y2K_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->millennium) {
			if (Y2001_TIME_START >= timeLocal)
				dp->str1pta = (char *) Y2001_STRING;
			else
				dp->str1pta = (char *) POST_Y2001_STRING;
			dp->str1ptb = dp->str1pta;
		} else {
			struct tm *t = localtime((const time_t *) &timeLocal);

			(void) strftime(dp->str1, STRSIZE, "%I:%M:%S %p", t);
			dp->hour = t->tm_hour;
			(void) strftime(dp->str2, STRSIZE, "%a %b %d %Y", t);

			dp->str1pta = dp->str1;
			dp->str1ptb = dp->str1old;
		}
		if (dp->popex) {
			convert(PEOPLE_TIME_START + (PEOPLE_MIN / 60.0) * timeNow, dp->str2);
			(void) strncat(dp->str2, PEOPLE_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->forest) {
			convert(AREA_TIME_START - (AREA_MIN / 60.0) * timeNow, dp->str2);
			(void) strncat(dp->str2, TROPICAL_STRING, STRSIZE-strlen(dp->str2));
			(void) strncat(dp->str2, AREA_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->hiv) {
			convert(HIV_TIME_START + (HIV_MIN / 60.0) * timeNow, dp->str2);
			(void) strncat(dp->str2, CASES_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->lab) {
			convert((LAB_MIN / 60.0) * (timeNow - timeAtLastNewYear(timeNow)),
				dp->str2);
			(void) strncat(dp->str2, YEAR_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->veg) {
			convert((VEG_MIN / 60.0) * (timeNow - timeAtLastNewYear(timeNow)),
				dp->str2);
			(void) strncat(dp->str2, YEAR_STRING, STRSIZE-strlen(dp->str2));
			dp->str2pta = dp->str2;
		} else if (dp->y2k) {
			dayhrminsec(Y2K_TIME_START - timeLocal, dp->tzoffset, dp->str2);
			dp->str2pta = dp->str2;
		} else if (dp->millennium) {
			dayhrminsec(Y2001_TIME_START - timeLocal, dp->tzoffset, dp->str2);
			dp->str2pta = dp->str2;
		} else {
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2old;
		}

		dp->text_height = font_height(mode_font);
#ifdef USE_MB
		{
			XRectangle ink, logical;

			XmbTextExtents(mode_font, dp->str1pta, strlen(dp->str1pta), &ink, &logical);
			dp->text_width1 = logical.width;
			XmbTextExtents(mode_font, dp->str2pta, strlen(dp->str2pta), &ink, &logical);
			dp->text_width2 = logical.width;
		}
#else
		dp->text_width1 = XTextWidth(mode_font, dp->str1pta, strlen(dp->str1pta));
		dp->text_width2 = XTextWidth(mode_font, dp->str2pta, strlen(dp->str2pta));
#endif
		if (dp->text_width1 > dp->text_width2) {
			dp->text_width = dp->text_width1;
			dp->text_start1 = 0;
			dp->text_start2 = (dp->text_width - dp->text_width2) / 2;
		} else {
			dp->text_width = dp->text_width2;
			dp->text_start1 = (dp->text_width - dp->text_width1) / 2;
			dp->text_start2 = 0;
		}
		dp->maxy = dp->height - dp->text_height - dp->text_descent;
		if (dp->maxy - dp->text_ascent == 0)
			dp->clocky = dp->text_ascent;
		else if (dp->maxy - dp->text_ascent < 0)
			dp->clocky = -NRAND(dp->text_ascent - dp->maxy) + dp->text_ascent;
		else
			dp->clocky = NRAND(dp->maxy - dp->text_ascent) + dp->text_ascent;
	}
	dp->maxx = dp->width - dp->text_width;
	if (dp->maxx == 0)
		dp->clockx = dp->text_start1;
	else if (dp->maxx < 0)
		dp->clockx = -NRAND(-dp->maxx) + dp->text_start1;
	else
		dp->clockx = NRAND(dp->maxx) + dp->text_start1;

	if (MI_NPIXELS(mi) > 2)
		dp->color = NRAND(MI_NPIXELS(mi));

	/* don't want any exposure events from XCopyPlane */
	XSetGraphicsExposures(display, MI_GC(mi), False);
}

void
draw_dclock(ModeInfo * mi)
{
	dclockstruct *dp;

	if (dclocks == NULL)
		return;
	dp = &dclocks[MI_SCREEN(mi)];
	if (mode_font == None)
		return;

	MI_IS_DRAWN(mi) = True;
	drawDclock(mi);
	if (MI_NPIXELS(mi) > 2) {
		if (++dp->color >= MI_NPIXELS(mi))
			dp->color = 0;
	}
}

void
refresh_dclock(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_dclock */
