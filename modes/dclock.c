/* -*- Mode: C; tab-width: 4 -*- */
/* dclock --- floating digital clock or message */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)dclock.c	4.07 97/11/24 xlockmore";

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
 * 07-May-99: New "savers" added for y2k and second millennium countdowns.
 *            Tom Schmidt <tschmidt@micron.com>
 * 04-Dec-98: New "savers" added for hiv, veg, and lab.
 *            hiv one due to Kenneth Stailey <kstailey@disclosure.com>
 * 10-Aug-98: Population Explosion and Tropical Forest Countdown stuff
 *            I tried to get precise numbers but they may be off a few percent.
 *            Whether or not, its still pretty scary IMHO.
 *            Although I am a US citizen... I have the default for area in
 *            metric.  David Bagley <bagleyd@tux.org>
 * 10-May-97: Compatible with xscreensaver
 * 29-Aug-95: Written.
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
#ifdef NL
# define AREA_STRING "Hectare"
#else
# define AREA_STRING "Hectares"
#endif
#define AREA_MIN 35.303144
#define AREA_TIME_START 1184193000.0
#else
#ifdef NL
# define AREA_STRING "Acre"
#else
# define AREA_STRING "Acres"
#endif
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
	{"-led", ".dclock.led", XrmoptionNoArg, (caddr_t) "on"},
	{"+led", ".dclock.led", XrmoptionNoArg, (caddr_t) "off"},
	{"-popex", ".dclock.popex", XrmoptionNoArg, (caddr_t) "on"},
	{"+popex", ".dclock.popex", XrmoptionNoArg, (caddr_t) "off"},
	{"-forest", ".dclock.forest", XrmoptionNoArg, (caddr_t) "on"},
	{"+forest", ".dclock.forest", XrmoptionNoArg, (caddr_t) "off"},
	{"-hiv", ".dclock.hiv", XrmoptionNoArg, (caddr_t) "on"},
	{"+hiv", ".dclock.hiv", XrmoptionNoArg, (caddr_t) "off"},
	{"-lab", ".dclock.lab", XrmoptionNoArg, (caddr_t) "on"},
	{"+lab", ".dclock.lab", XrmoptionNoArg, (caddr_t) "off"},
	{"-veg", ".dclock.veg", XrmoptionNoArg, (caddr_t) "on"},
	{"+veg", ".dclock.veg", XrmoptionNoArg, (caddr_t) "off"},
	{"-y2k", ".dclock.y2k", XrmoptionNoArg, (caddr_t) "on"},
	{"+y2k", ".dclock.y2k", XrmoptionNoArg, (caddr_t) "off"},
	{"-millennium", ".dclock.millennium", XrmoptionNoArg, (caddr_t) "on"},
	{"+millennium", ".dclock.millennium", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & led, "led", "LED", DEF_LED, t_Bool},
	{(caddr_t *) & popex, "popex", "PopEx", DEF_POPEX, t_Bool},
	{(caddr_t *) & forest, "forest", "Forest", DEF_FOREST, t_Bool},
	{(caddr_t *) & hiv, "hiv", "Hiv", DEF_HIV, t_Bool},
	{(caddr_t *) & lab, "lab", "Lab", DEF_LAB, t_Bool},
	{(caddr_t *) & veg, "veg", "Veg", DEF_VEG, t_Bool},
	{(caddr_t *) & y2k, "y2k", "Y2K", DEF_Y2K, t_Bool},
	{(caddr_t *) & millennium, "millennium", "Millennium", DEF_Y2001, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+led", "turn on/off Light Emitting Diode seven segment display"},
	{"-/+popex", "turn on/off population explosion counter"},
	{"-/+forest", "turn on/off tropical forest destruction counter"},
	{"-/+hiv", "turn on/off HIV infection counter"},
	{"-/+lab", "turn on/off Animal Research counter"},
	{"-/+veg", "turn on/off Animal Consumation counter"},
	{"-/+y2k", "turn on/off Year 2000 countdown"},
	{"-/+millennium", "turn on/off 3rd Millennium (1 January 2001) countdown"},
};

ModeSpecOpt dclock_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   dclock_description =
{"dclock", "init_dclock", "draw_dclock", "release_dclock",
 "refresh_dclock", "init_dclock", NULL, &dclock_opts,
 10000, 1, 10000, 1, 64, 0.3, "",
 "Shows a floating digital clock or message", 0, NULL};

#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <time.h>

#ifdef FR
#define POPEX_STRING "Population mondiale"
#define PEOPLE_STRING " personnes"
#define FOREST_STRING "Taille des forêts tropicales"
#define TROPICAL_STRING " zones tropicales en "
#define HIV_STRING "Infections par le SIDA dans le monde"
#define CASES_STRING " cas"
#define LAB_STRING "Animaux utilisés dans la recherche"
#define VEG_STRING "Animaux consommés comme aliments par l'homme"
#define YEAR_STRING " cette année"
#define Y2K_STRING "Décompte pour l'an 2000 (1er Janvier 2000)"
#define POST_Y2K_STRING "Temps depuis l'an 2000 (1er Janvier 2000)"
#define Y2001_STRING "Décompte pour le Second Millénaire (1er Janvier 2001)"
#define POST_Y2001_STRING "Temps depuis le Second Millénaire (1er Janvier 2001)"
#define DAY "jour"
#define DALED_YS "jours"
#define HOUR "heure"
#define HOURS "heures"
#define MINUTE "minute"
#define MINUTES "minutes"
#define SECOND "seconde"
#define SECONDS "secondes"
#else
#ifdef NL
#define POPEX_STRING "Wereld populatie"
#define PEOPLE_STRING " Mensen"
#define FOREST_STRING "Aantal tropische wouden"
#define TROPICAL_STRING " Tropisch gebied in "
#define HIV_STRING "Huidige staat HIV infecties wereldwijd"
#define CASES_STRING " gevallen"
#define LAB_STRING "Verbruikt tijdens onderzoek"
#define VEG_STRING "Opgegeten door de mens"
#define YEAR_STRING " dieren dit jaar"
#define Y2K_STRING "Aftellen tot Y2K (1 Januari 2000, 0.00 uur)"
#define POST_Y2K_STRING "Tijd sinds Y2K (1 Januari 2000)"
#define Y2001_STRING "Aftellen tot het einde van het tweede Millennium (1 Januari 2001, 0.00 uur)"
#define POST_Y2001_STRING "Verstreken tijd sinds het begin van het derde Millennium (1 Januari 2001)"

#define Y2001_STRING "Aftellen tot het tweede millennium (1 Januari 2001)"
#define POST_Y2001_STRING "Tijd sinds het tweede millennium (1 Januari 2001)"
#define DAY "dag"
#define DALED_YS "dagen"
#define HOUR "uur"
#define HOURS "uren"
#define MINUTE "minuut"
#define MINUTES "minuten"
#define SECOND "seconde"
#define SECONDS "seconden"
#else
#define POPEX_STRING "World Population"
#define PEOPLE_STRING " People"
#define FOREST_STRING "Tropical Forest Countdown"
#define TROPICAL_STRING " Tropical Forest in "
#define HIV_STRING "Current HIV Infections World Wide"
#define CASES_STRING " Cases"
#define LAB_STRING "Used in research"
#define VEG_STRING "Consumed for Food by Mankind"
#define YEAR_STRING " animals this year"
#define Y2K_STRING "Countdown to Y2K (1 January 2000, 0:00 hour)"
#define POST_Y2K_STRING "Time since Y2K (1 January 2000)"
#define Y2001_STRING "Countdown to the end of the Second Millennium (1 January 2001, 0:00 hour)"
#define POST_Y2001_STRING "Time since the start of the Third Millennium (1 January 2001)"
#define DAY "day"
#define DALED_YS "days"
#define HOUR "hour"
#define HOURS "hours"
#define MINUTE "minute"
#define MINUTES "minutes"
#define SECOND "second"
#define SECONDS "seconds"
#endif
#endif

#define font_height(f) (f->ascent + f->descent)

extern XFontStruct *getFont(Display * display);

typedef struct {
	int         color;
	short       height, width;
	char       *str, str1[40], str2[40], str1old[80], str2old[80];
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

static dclockstruct *dclocks = NULL;

static XFontStruct *mode_font = None;

#define BASE 10.0
#define GROUP 3
static double logbase;

static unsigned long
timeAtLastNewYear(unsigned long timeNow)
{
  int year = 1970, days; /* Beginning of time */
	unsigned long  lastNewYear = 0;
	unsigned long secondCount = 0;

	while (lastNewYear + secondCount < timeNow) {
		lastNewYear += secondCount;
		days = 365;
		if ((!(year % 400)) || ((year % 100) && (!(year % 4))))
			days++;
		secondCount = days * 24 * 60 * 60;
		year++;
	}
	return lastNewYear;
}

static void
convert(double x, char *string)
{
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
	string[k++] = '\0';
}

static void
dayhrminsec(long timeCount, int tzoffset, char *string)
{
	int	 days, hours, minutes, secs;
	char	*buf;

	timeCount = abs(timeCount);
	days = timeCount / 86400;
	hours = (timeCount / 3600) % 24;
	minutes = (timeCount / 60) % 60;
	secs = timeCount % 60;
	buf = (char *) malloc(16);
	(void) sprintf(string, "%d ", days);
	if (days == 1)
		(void) strcat(string, DAY);
	else
		(void) strcat(string, DALED_YS);
	(void) sprintf(buf, ", %d ", hours);
	(void) strcat(string, buf);
	if (hours == 1)
		(void) strcat(string, HOUR);
	else
		(void) strcat(string, HOURS);
	(void) sprintf(buf, ", %d ", minutes);
	(void) strcat(string, buf);
	if (minutes == 1)
		(void) strcat(string, MINUTE);
	else
		(void) strcat(string, MINUTES);
	(void) sprintf(buf, ", %d ", secs);
	(void) strcat(string, buf);
	if (secs == 1)
		(void) strcat(string, SECOND);
	else
		(void) strcat(string, SECONDS);
	if (!tzoffset)
		(void) strcat(string, " (UTC)");
	(void) free((void *) buf);
	buf = NULL;
}

static void
drawaled(ModeInfo * mi, int startx, int starty, int led)
{
	Display    *display = MI_DISPLAY(mi);
	dclockstruct *dp = &dclocks[MI_SCREEN(mi)];
 	int x_1, y_1, x_2, y_2;

	int offset = (int) LED_WIDTH;
	int offset2 = (int) (LED_WIDTH / 2.0);
	int leanoffset = (int) (offset2 * LED_LEAN);

	switch (led) {
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
	int led;

	for (led = 0; led < MAX_LEDS; led++) {
		if (digits[digit][led])
			drawaled(mi, startx, starty, led);
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
	char       *tmppt;

	if (dp->led) {
		dp->timeold = dp->timenew = time((time_t *) NULL);
		dp->str = ctime(&dp->timeold);
	} else if (!dp->popex && !dp->forest && !dp->hiv &&
	    !dp->lab && !dp->veg && !dp->y2k && !dp->millennium) {
		if (dp->timeold != (dp->timenew = time((time_t *) NULL))) {
			/* only parse if time has changed */
			dp->timeold = dp->timenew;
			dp->str = ctime(&dp->timeold);

			if (!dp->popex && !dp->forest && !dp->hiv && !dp->lab &&
			    !dp->veg && !dp->y2k && !dp->millennium) {

				/* keep last disp time so it can be cleared even if it changed */
				tmppt = dp->str1ptb;
				dp->str1ptb = dp->str1pta;
				dp->str1pta = tmppt;

				/* copy the hours portion for 24 to 12 hr conversion */
				(void) strncpy(dp->str1pta, (dp->str + 11), 8);
				dp->hour = (short) (dp->str1pta[0] - 48) * 10 +
					(short) (dp->str1pta[1] - 48);
				if (dp->hour > 12) {
					dp->hour -= 12;
					(void) strcpy(dp->str1pta + 8, " PM");
				} else {
					if (dp->hour == 0)
						dp->hour += 12;
					(void) strcpy(dp->str1pta + 8, " AM");
				}
				dp->str1pta[0] = (dp->hour / 10) + 48;
				dp->str1pta[1] = (dp->hour % 10) + 48;
				if (dp->str1pta[0] == '0')
					dp->str1pta[0] = ' ';
				/* keep last disp time so it can be cleared even if it changed */
			}
			tmppt = dp->str2ptb;
			dp->str2ptb = dp->str2pta;
			dp->str2pta = tmppt;

			/* copy day month */
			(void) strncpy(dp->str2pta, dp->str, 11);
			/* copy year */
			(void) strncpy(dp->str2pta + 11, (dp->str + 20), 4);
		}
	} else {
		unsigned long timeNow, timeLocal;
		timeNow = seconds();
		timeLocal = timeNow - dp->tzoffset;

		if (dp->popex) {
			convert(PEOPLE_TIME_START + (PEOPLE_MIN / 60.0) * timeNow, dp->str2);
			(void) strcat(dp->str2, PEOPLE_STRING);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->forest) {
			convert(AREA_TIME_START - (AREA_MIN / 60.0) * timeNow, dp->str2);
			(void) strcat(dp->str2, TROPICAL_STRING);
			(void) strcat(dp->str2, AREA_STRING);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->hiv) {
			convert(HIV_TIME_START + (HIV_MIN / 60.0) * timeNow, dp->str2);
			(void) strcat(dp->str2, CASES_STRING);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->lab) {
			convert((LAB_MIN / 60.0) * (timeNow - timeAtLastNewYear(timeNow)),
				dp->str2);
			(void) strcat(dp->str2, YEAR_STRING);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->veg) {
			convert((VEG_MIN / 60.0) * (timeNow - timeAtLastNewYear(timeNow)),
				dp->str2);
			(void) strcat(dp->str2, YEAR_STRING);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->y2k) {
			if (Y2K_TIME_START >= timeLocal)
				dp->str1pta = Y2K_STRING;
			else
				dp->str1pta = POST_Y2K_STRING;
			dp->str1ptb = dp->str1pta;
			dayhrminsec(Y2K_TIME_START - timeLocal, dp->tzoffset, dp->str2);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->millennium) {
			if (Y2001_TIME_START >= timeLocal)
				dp->str1pta = Y2001_STRING;
			else
				dp->str1pta = POST_Y2001_STRING;
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
		dp->text_width1 = XTextWidth(mode_font, dp->str1pta, strlen(dp->str1pta));
		dp->text_width2 = XTextWidth(mode_font, dp->str2pta, strlen(dp->str2pta));
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

		if (dp->fgGC)
			XFreeGC(display, dp->fgGC);
		if (dp->bgGC)
			XFreeGC(display, dp->bgGC);
		if (dp->pixmap) {
			XFreePixmap(display, dp->pixmap);
			MI_CLEARWINDOWCOLORMAPFAST(mi, gc, MI_BLACK_PIXEL(mi));
		}
		dp->pixw = dp->text_width;
		if (dp->led)
			dp->pixh = dp->text_height;
		else
			dp->pixh = 2 * dp->text_height;
		dp->pixmap = XCreatePixmap(display, window, dp->pixw, dp->pixh, 1);
		gcv.font = mode_font->fid;
		gcv.background = 0;
		gcv.foreground = 1;
		gcv.graphics_exposures = False;
		dp->fgGC = XCreateGC(display, dp->pixmap,
				     GCForeground | GCBackground | GCGraphicsExposures | GCFont, &gcv);
		gcv.foreground = 0;
		dp->bgGC = XCreateGC(display, dp->pixmap,
				     GCForeground | GCBackground | GCGraphicsExposures | GCFont, &gcv);
		XSetLineAttributes(MI_DISPLAY(mi), dp->fgGC,
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
		(void) XDrawString(display, dp->pixmap, dp->fgGC,
			   dp->text_start1, dp->text_ascent,
			   dp->str1pta, strlen(dp->str1pta));
		(void) XDrawString(display, dp->pixmap, dp->fgGC,
			dp->text_start2, dp->text_ascent + dp->text_height,
			   dp->str2pta, strlen(dp->str2pta));
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
		   0, 0, dp->text_width, 2 * dp->text_height,
		dp->clockx - dp->text_start1, dp->clocky - dp->text_ascent,
		   1L);
}

void
init_dclock(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	dclockstruct *dp;
	unsigned long timeNow, timeLocal;
#if defined(HAVE_TZSET) && !(defined(BSD) && BSD >= 199306)
	extern long timezone;
#endif

	if (dclocks == NULL) {
		logbase = log(BASE);
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

	if (mode_font == None)
		mode_font = getFont(display);
	if (!dp->done) {
		dp->done = 1;
		if (mode_font != None)
			XSetFont(display, MI_GC(mi), mode_font->fid);
	}
	/* (void)time(&dp->timenew); */
#if defined(HAVE_TZSET) && (!defined(HAVE_TIMELOCAL) || (defined(BSD) && BSD >= 199306))
	(void) tzset();
#endif
	dp->timeold = dp->timenew = time((time_t *) NULL);
#if defined(HAVE_TIMELOCAL) && !(defined(BSD) && BSD >= 199306)
	if (!dp->tzoffset)
		dp->tzoffset = timelocal(&dp->timeold) - timegm(&dp->timeold);
#else
#ifdef HAVE_TZSET
	dp->tzoffset = (int)timezone;
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
		dp->text_descent = mode_font->descent;
		dp->text_ascent = mode_font->ascent;
		if (dp->popex) {
			dp->str1pta = POPEX_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->forest) {
			dp->str1pta = FOREST_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->hiv) {
			dp->str1pta = HIV_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->lab) {
			dp->str1pta = LAB_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->veg) {
			dp->str1pta = VEG_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->y2k) {
			if (Y2K_TIME_START >= timeLocal)
				dp->str1pta = Y2K_STRING;
			else
				dp->str1pta = POST_Y2K_STRING;
			dp->str1ptb = dp->str1pta;
		} else if (dp->millennium) {
			if (Y2001_TIME_START >= timeLocal)
				dp->str1pta = Y2001_STRING;
			else
				dp->str1pta = POST_Y2001_STRING;
			dp->str1ptb = dp->str1pta;
		} else {
			(void) strncpy(dp->str1, (dp->str + 11), 8);
			dp->hour = (short) (dp->str1[0] - 48) * 10 + (short) (dp->str1[1] - 48);
			if (dp->hour > 12) {
				dp->hour -= 12;
				(void) strcpy(dp->str1 + 8, " PM");
			} else {
				if (dp->hour == 0)
					dp->hour += 12;
				(void) strcpy(dp->str1 + 8, " AM");
			}
			dp->str1[0] = (dp->hour / 10) + 48;
			dp->str1[1] = (dp->hour % 10) + 48;
			if (dp->str1[0] == '0')
				dp->str1[0] = ' ';
			dp->str1[11] = 0;	/* terminate dp->str1 */
			dp->str1old[11] = 0;	/* terminate dp->str1old */

			(void) strncpy(dp->str2, dp->str, 11);
			(void) strncpy(dp->str2 + 11, (dp->str + 20), 4);
			dp->str2[15] = 0;	/* terminate dp->str2 */
			dp->str2old[15] = 0;	/* terminate dp->str2old */

			dp->str1pta = dp->str1;
			dp->str1ptb = dp->str1old;
		}
		if (dp->popex) {
			convert(PEOPLE_TIME_START + (PEOPLE_MIN / 60.0) * timeNow, dp->str2);
			(void) strcat(dp->str2, PEOPLE_STRING);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->forest) {
			convert(AREA_TIME_START - (AREA_MIN / 60.0) * timeNow, dp->str2);
			(void) strcat(dp->str2, TROPICAL_STRING);
			(void) strcat(dp->str2, AREA_STRING);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->hiv) {
			convert(HIV_TIME_START + (HIV_MIN / 60.0) * timeNow, dp->str2);
			(void) strcat(dp->str2, CASES_STRING);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->lab) {
			convert((LAB_MIN / 60.0) * (timeNow - timeAtLastNewYear(timeNow)),
				dp->str2);
			(void) strcat(dp->str2, YEAR_STRING);
			dp->str2pta = dp->str2;
			dp->str2ptb = dp->str2pta;
		} else if (dp->veg) {
			convert((VEG_MIN / 60.0) * (timeNow - timeAtLastNewYear(timeNow)),
				dp->str2);
			(void) strcat(dp->str2, YEAR_STRING);
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
		dp->text_width1 = XTextWidth(mode_font, dp->str1pta, strlen(dp->str1pta));
		dp->text_width2 = XTextWidth(mode_font, dp->str2pta, strlen(dp->str2pta));
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
	dclockstruct *dp = &dclocks[MI_SCREEN(mi)];

	MI_IS_DRAWN(mi) = True;

	drawDclock(mi);
	if (MI_NPIXELS(mi) > 2) {
		if (++dp->color >= MI_NPIXELS(mi))
			dp->color = 0;
	}
}

void
release_dclock(ModeInfo * mi)
{
	if (dclocks != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			dclockstruct *dp = &dclocks[screen];
			Display    *display = MI_DISPLAY(mi);

			if (dp->fgGC)
				XFreeGC(display, dp->fgGC);
			if (dp->bgGC)
				XFreeGC(display, dp->bgGC);
			if (dp->pixmap)
				XFreePixmap(display, dp->pixmap);

		}
		(void) free((void *) dclocks);
		dclocks = NULL;
	}
	if (mode_font != None) {
		XFreeFont(MI_DISPLAY(mi), mode_font);
		mode_font = None;
	}
}

void
refresh_dclock(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
}

#endif /* MODE_dclock */
