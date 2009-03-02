/* -*- Mode: C; tab-width: 4 -*- */
/* dclock --- floating digital clock */

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
 * 04-Dec-98: New "savers" added for hiv, veg, and lab.
              hiv one due to Kenneth Stailey <kstailey@disclosure.com>
 * 10-Aug-98: Population Explosion and Tropical Forest Countdown stuff
 *            I tried to get precise numbers but they may be off a few percent.
 *            Whether or not, its still pretty scary IMHO. 
 *            Although I am a US citizen... I have the default for area in
 *            metric.  David Bagley <bagleyd@bigfoot.com>
 * 10-May-97: Compatible with xscreensaver
 * 29-Aug-95: Written.
 */

/*-
 *  Some of my calcualations laid bare...  (I have a little problem with
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
 *  HIV Infection Counter Saver (hiv)
 *  -130,828,080 cases (figured by extrapolation) 1 Jan 1970
 *  (hmmm...)
 *  36,503,000 3 Dec 1998 (from http://www.vers.com/aidsclock/INDEXA.HTM)
 *  11 cases/min
 *
 *  365.25 * 24 * 60 =  525960 minutes in a year
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
#define AREA_STRING "Hectares"
#define AREA_MIN 35.303144
#define AREA_TIME_START 1184193000.0
#else
#define AREA_STRING "Acres"
#define AREA_MIN 87.198766
#define AREA_TIME_START 2924959000.0
#endif
#define PEOPLE_MIN 160.70344
#define PEOPLE_TIME_START 3535369000.0
#define HIV_MIN 11.0
#define HIV_TIME_START -130828080.0
#define LAB_MIN 32.32184957
#define LAB_TIME_START 0
#define VEG_MIN 9506.426344209
#define VEG_TIME_START 0

#define DEF_POPEX  "False"
#define DEF_FOREST "False"
#define DEF_HIV    "False"
#define DEF_LAB    "False"
#define DEF_VEG    "False"

static Bool popex;
static Bool forest;
static Bool hiv;
static Bool lab;
static Bool veg;

static XrmOptionDescRec opts[] =
{
	{"-popex", ".dclock.popex", XrmoptionNoArg, (caddr_t) "on"},
	{"+popex", ".dclock.popex", XrmoptionNoArg, (caddr_t) "off"},
	{"-forest", ".dclock.forest", XrmoptionNoArg, (caddr_t) "on"},
	{"+forest", ".dclock.forest", XrmoptionNoArg, (caddr_t) "off"},
	{"-hiv", ".dclock.hiv", XrmoptionNoArg, (caddr_t) "on"},
	{"+hiv", ".dclock.hiv", XrmoptionNoArg, (caddr_t) "off"},
	{"-lab", ".dclock.lab", XrmoptionNoArg, (caddr_t) "on"},
	{"+lab", ".dclock.lab", XrmoptionNoArg, (caddr_t) "off"},
	{"-veg", ".dclock.veg", XrmoptionNoArg, (caddr_t) "on"},
	{"+veg", ".dclock.veg", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & popex, "popex", "PopEx", DEF_POPEX, t_Bool},
	{(caddr_t *) & forest, "forest", "Forest", DEF_FOREST, t_Bool},
	{(caddr_t *) & hiv, "hiv", "Hiv", DEF_HIV, t_Bool},
	{(caddr_t *) & lab, "lab", "Lab", DEF_LAB, t_Bool},
	{(caddr_t *) & veg, "veg", "Veg", DEF_VEG, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+popex", "turn on/off population explosion"},
	{"-/+forest", "turn on/off tropical forest descruction"},
	{"-/+hiv", "turn on/off HIV infection counter"},
	{"-/+lab", "turn on/off Animal Research counter"},
	{"-/+veg", "turn on/off Animal Consumation counter"}
};

ModeSpecOpt dclock_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   dclock_description =
{"dclock", "init_dclock", "draw_dclock", "release_dclock",
 "refresh_dclock", "init_dclock", NULL, &dclock_opts,
 10000, 1, 10000, 1, 0.3, 64, "",
 "Shows a floating digital clock", 0, NULL};

#endif

#include <time.h>

#ifdef FR
#define POPEX_STRING "Population Mondiale"
#define PEOPLE_STRING " Personnes"
#define FOREST_STRING "Nombre des Forets Tropicales"
#define TROPICAL_STRING " Zones Tropicales en "
#define HIV_STRING "Infection par le HIV a l'heure actuelle dans le monde est de"
#define CASES_STRING ""
#define LAB_STRING "Used in research"
#define VEG_STRING "Consumed for Food by Mankind"
#define YEAR_STRING " animals this year"
#else
#ifdef NL
#define POPEX_STRING "Wereld populatie"
#define PEOPLE_STRING " Mensen"
#define FOREST_STRING "Aantal tropische wouden"
#define TROPICAL_STRING " Tropisch gebied in "
#define HIV_STRING "Huidige staat HIV infecties wereldwijd"
#define CASES_STRING " gevallen"
#define LAB_STRING "Used in research"
#define VEG_STRING "Consumed for Food by Mankind"
#define YEAR_STRING " animals this year"
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
	short       maxx, maxy, clockx, clocky;
	short       text_height, text_width, text_width1, text_width2;
	short       text_start1, text_start2;
	short       hour;
	short       dx, dy;
	int         done;
	int         pixw, pixh;
	Pixmap      pixmap;
	GC          fgGC, bgGC;
	Bool        popex, forest, hiv, lab, veg;
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
drawDclock(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	dclockstruct *dp = &dclocks[MI_SCREEN(mi)];
	short       xold, yold;
	char       *tmppt;

	xold = dp->clockx;
	yold = dp->clocky;
	dp->clockx += dp->dx;
	dp->clocky += dp->dy;

	if (dp->maxx < dp->text_start1) {
		if (dp->clockx < dp->maxx + dp->text_start1 ||
		    dp->clockx > dp->text_start1) {
			dp->dx = -dp->dx;
			dp->clockx += dp->dx;
		}
	} else if (dp->maxx > dp->text_start1) {
		if (dp->clockx > dp->maxx + dp->text_start1 ||
		    dp->clockx < dp->text_start1) {
			dp->dx = -dp->dx;
			dp->clockx += dp->dx;
		}
	}
	if (dp->maxy < mode_font->ascent) {
		if (dp->clocky > mode_font->ascent || dp->clocky < dp->maxy) {
			dp->dy = -dp->dy;
			dp->clocky += dp->dy;
		}
	} else if (dp->maxy > mode_font->ascent) {
		if (dp->clocky > dp->maxy || dp->clocky < mode_font->ascent) {
			dp->dy = -dp->dy;
			dp->clocky += dp->dy;
		}
	}
	if (!dp->popex && !dp->forest && !dp->hiv && !dp->lab && !dp->veg) {
		if (dp->timeold != (dp->timenew = time((time_t *) NULL))) {
			/* only parse if time has changed */
			dp->timeold = dp->timenew;
			dp->str = ctime(&dp->timeold);

			if (!dp->popex && !dp->forest && !dp->hiv && !dp->lab && !dp->veg) {

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
		unsigned long timeNow = seconds();

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
		}
	}
	if (dp->pixw != dp->text_width || dp->pixh != 2 * dp->text_height) {
		XGCValues   gcv;

		if (dp->fgGC)
			XFreeGC(display, dp->fgGC);
		if (dp->bgGC)
			XFreeGC(display, dp->bgGC);
		if (dp->pixmap) {
			XFreePixmap(display, dp->pixmap);
			MI_CLEARWINDOW(mi);
		}
		dp->pixw = dp->text_width;
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
	}
	XFillRectangle(display, dp->pixmap, dp->bgGC, 0, 0, dp->pixw, dp->pixh);

	(void) XDrawString(display, dp->pixmap, dp->fgGC,
			   dp->text_start1, mode_font->ascent,
			   dp->str1pta, strlen(dp->str1pta));
	(void) XDrawString(display, dp->pixmap, dp->fgGC,
			dp->text_start2, mode_font->ascent + dp->text_height,
			   dp->str2pta, strlen(dp->str2pta));
	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	/* This could leave screen dust on the screen if the width changes
	   But that only happens once a day...
	   ... this is solved by the ClearWindow above
	 */
	ERASE_IMAGE(display, window, gc,
	    (dp->clockx - dp->text_start1), (dp->clocky - mode_font->ascent),
		    (xold - dp->text_start1), (yold - mode_font->ascent),
		    dp->pixw, dp->pixh);
	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, gc, MI_PIXEL(mi, dp->color));
	else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	XCopyPlane(display, dp->pixmap, window, gc,
		   0, 0, dp->text_width, 2 * dp->text_height,
		dp->clockx - dp->text_start1, dp->clocky - mode_font->ascent,
		   1L);
}

void
init_dclock(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	dclockstruct *dp;
	unsigned long timeNow;

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

	dp->popex = False;
	dp->forest = False;
	dp->hiv = False;
	dp->lab = False;
	dp->veg = False;
	if (MI_IS_FULLRANDOM(mi)) {
		switch (NRAND(6)) {
			case 1:
				dp->popex = True;
				break;
			case 2:
				dp->forest = True;
				break;
			case 3:
				dp->hiv = True;
				break;
			case 4:
				dp->lab = True;
				break;
			case 5:
				dp->veg = True;
				break;
			default:
				break;
		}
	} else { /* first come, first served */
		dp->popex = popex;
		dp->forest = forest;
		dp->hiv = hiv;
		dp->lab = lab;
		dp->veg = veg;
	}

	if (mode_font == None)
		mode_font = getFont(display);
	if (!dp->done) {
		dp->done = 1;
		if (mode_font != None)
			XSetFont(display, MI_GC(mi), mode_font->fid);
	}
	/* (void)time(&dp->timenew); */
	dp->timeold = dp->timenew = time((time_t *) NULL);
	dp->str = ctime(&dp->timeold);
	dp->dx = (LRAND() & 1) ? 1 : -1;
	dp->dy = (LRAND() & 1) ? 1 : -1;

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
	timeNow = seconds();
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
	dp->maxx = dp->width - dp->text_width;
	dp->maxy = dp->height - dp->text_height - mode_font->descent;
	if (dp->maxx == 0)
		dp->clockx = dp->text_start1;
	else if (dp->maxx < 0)
		dp->clockx = -NRAND(-dp->maxx) + dp->text_start1;
	else
		dp->clockx = NRAND(dp->maxx) + dp->text_start1;
	if (dp->maxy - mode_font->ascent == 0)
		dp->clocky = mode_font->ascent;
	else if (dp->maxy - mode_font->ascent < 0)
		dp->clocky = -NRAND(mode_font->ascent - dp->maxy) + mode_font->ascent;
	else
		dp->clocky = NRAND(dp->maxy - mode_font->ascent) + mode_font->ascent;

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
