/* -*- Mode: C; tab-width: 4 -*- */
/* toneclock --- Screenhack inspired by Peter Schat's toneclock
 */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)toneclock.c	5.00 2004/09/16 xlockmore";

#endif

/*-
 * Copyright (c) 2004 by Jouk Jansen <joukj@hrem.stm.tudelft.nl>
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
 * The author should like to be notified if changes have been made to the
 * routine.  Response will only be guaranteed when a VMS version of the
 * program is available.
 *
 * Toneclock. Peter Schat (1937-2003) was a Dutch composer. Het invented
 * the toneclock, a garphical representation of the tonesystem.
 *
 * Revision History:
 * 20-Dec-2004: Initial release
 * 23-Dec-2004: -size determines size of the clock
 *              -original ignores all options and displays original clock
 *              -pulsate make the clock pulsating
 * 24-Dec-2004: -fill num
 *               polygon filling chance implemented
 * 04-Jan-2005: -move allow moving for small clocks
 * 16-Feb-2005: -Number of hours according to "count" option
 *              -Random polygons
 *
 */
#if 0
#define NO_DBUF
#endif
#ifdef STANDALONE
#define MODE_toneclock
#define PROGCLASS "toneclock"
#define HACK_INIT init_toneclock
#define HACK_DRAW draw_toneclock
#define toneclock_opts xlockmore_opts
#define DEFAULTS "*delay: 60000 \n" \
 "*count: -20 \n" \
 "*cycles: 200 \n" \
 "*size: -1000 \n" \
 "*ncolors: 64 \n" \
 "*fullrandom: True \n" \
 "*verbose: False \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "color.h"
#endif /* STANDALONE */

#ifdef MODE_toneclock

#define DEF_NUM_hour 12

#define DEF_CYCLE "True"
#define DEF_ORIGINAL "False"
#define DEF_PULSATING "False"
#define DEF_MOVE "True"
#define DEF_FILL "0"

#define min(a,b) ((a) <= (b) ? (a) : (b))

static Bool cycle_p , original , pulsating , move_clock;
static int fill;

static XrmOptionDescRec opts[] =
{
	{(char *) "-fill", (char *) "tomecloc.fill", XrmoptionSepArg, (caddr_t) NULL},
	{(char *) "-cycle", (char *) ".toneclock.cycle", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+cycle", (char *) ".toneclock.cycle", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-original", (char *) ".toneclock.original", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+original", (char *) ".toneclock.original", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-move", (char *) ".toneclock.move", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+move", (char *) ".toneclock.move", XrmoptionNoArg, (caddr_t) "off"},
	{(char *) "-pulsating", (char *) ".toneclock.pulsating", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+pulsating", (char *) ".toneclock.pulsating", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(void *) & fill, (char *) "fill", (char *) "Fill", (char *) DEF_FILL, t_Int},
	{(void *) & cycle_p, (char *) "cycle", (char *) "Cycle", (char *) DEF_CYCLE, t_Bool},
	{(void *) & original, (char *) "original", (char *) "Original", (char *) DEF_ORIGINAL, t_Bool},
	{(void *) & move_clock, (char *) "move", (char *) "Move", (char *) DEF_MOVE, t_Bool},
	{(void *) & pulsating, (char *) "pulsating", (char *) "Pulsating", (char *) DEF_PULSATING, t_Bool}
};
static OptionStruct desc[] =
{
	{(char *) "-fill num", (char *) "polygon filling chance in %"},
	{(char *) "-/+cycle", (char *) "turn on/off colour cycling"},
	{(char *) "-/+original", (char *) "turn on/off displaying original clock only"},
	{(char *) "-/+move", (char *) "turn on/off moving small clocks"},
	{(char *) "-/+pulsating", (char *) "turn on/off pulsating clock"}
};

ModeSpecOpt toneclock_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   toneclock_description =
{"toneclock", "init_toneclock", "draw_toneclock", "release_toneclock",
 "refresh_toneclock", "init_toneclock", (char *) NULL, &toneclock_opts,
 60000, -20, 200, -1000, 64, 1.0, "",
 "Shows Peter Schat's toneclock", 0, NULL};

#endif

#define PI_RAD (M_PI / 180.0)

typedef struct {
	unsigned long colour;
	float       angle, velocity , radius;
	int         num_point , num_point1;
        int*        point_numbers;
   	Bool draw;
} toneclockhour;

typedef struct {
	Bool        painted;
	int         win_width, win_height, num_hour, x0, y0 , fill;
	toneclockhour *hour;
        float       angle , velocity , radius , phase , ph_vel;
        float       vx , vy , anglex , angley , max_radius;
        int         a_x , a_y;
	GC          gc;
	Colormap    cmap;
	XColor     *colors;
	int         ncolors;
	Bool        cycle_p, mono_p, no_colors, original, pulsating , moving;
        Bool        randomhour;
	unsigned long blackpixel, whitepixel, fg, bg;
	int         direction;
	Pixmap      dbuf;
	GC          dbuf_gc;
	int         hexadecimal_clock[256];	
	ModeInfo   *mi;
} toneclockstruct;

static int original_clock[12][16] = {
     { 1 , 1 , 5 , 9 , 2 , 2 , 6  , 10 , 3 , 3 , 7 , 11 , 4 , 4 , 8 , 12 } ,
     { 1 , 1 , 2 , 3 , 4 , 4 , 5  , 6 , 7 , 7 , 8 , 9 , 10 , 10 , 11 , 12 } ,
     { 1 , 1 , 2 , 4 , 3 , 3 , 5  , 6 , 7 , 7 , 8 , 10 , 9 , 9 , 11 , 12 } ,
     { 1 , 1 , 2 , 5 , 3 , 3 , 4  , 7 , 6 , 6 , 9 , 10 , 8 , 8 , 11 , 12 } ,
     { 1 , 1 , 2 , 6 , 3 , 3 , 4  , 8 , 5 , 5 , 9 , 10 , 7 , 7 , 11 , 12 } ,
     { 1 , 1 , 2 , 7 , 3 , 3 , 8  , 9 , 4 , 4 , 5 , 10 , 6 , 6 , 11 , 12 } ,
     { 1 , 1 , 3 , 5 , 2 , 2 , 10 , 12 , 4 , 4 , 6 , 8 , 7 , 7 , 9 , 11 } ,
     { 1 , 1 , 3 , 6 , 2 , 2 , 4  , 11 , 5 , 5 , 8 , 10 , 7 , 7 , 9 , 12 } ,
     { 1 , 1 , 3 , 7 , 2 , 2 , 4  , 8 , 5 , 5 , 10 , 11 , 6 , 6 , 10 , 12 } ,
     { 1 , 1 , 3 , 8 , 2 , 2 , 7  , 9 , 4 , 4 , 6 , 11 , 5 , 5 , 10 , 12 } ,
     { 1 , 4 , 7 , 10 , 2 , 5 , 8  , 11 , 3 , 6 , 9 , 12 , 1 , 1 , 1 , 1 } ,
     { 1 , 1 , 4 , 8 , 2 , 2 , 7  , 11 , 3 , 3 , 6 , 10 , 5 , 5 , 9 , 12 }
};

static toneclockstruct *toneclocks = (toneclockstruct *) NULL;

static void
toneclock_drawhour(ModeInfo * mi, toneclockhour * hour0 ,
		   int x0 , int y0 )
{
	Display    *display = MI_DISPLAY(mi);
   	toneclockstruct *tclock;
	int i;
#ifdef NO_DBUF
	Window drawable = MI_WINDOW(mi);
#else
#define drawable tclock->dbuf
#endif
	tclock = &toneclocks[MI_SCREEN(mi)];

   for (i = 0; i < hour0->num_point1; i = i + 4)
     {
	int x , y , x1 , y1 , x2 , y2 , x3 , y3;

	x = (int) (hour0->radius * sin( hour0->angle + PI_RAD *
					hour0->point_numbers[ i ] * 360.0 /
					hour0->num_point ) ) + x0;
	y = (int) (hour0->radius * cos( hour0->angle + PI_RAD *
					hour0->point_numbers[ i ] * 360.0 /
					hour0->num_point ) ) + y0;
	x1 = (int) (hour0->radius * sin( hour0->angle + PI_RAD *
					hour0->point_numbers[ i + 1 ] * 360.0 /
					hour0->num_point ) ) + x0;
	y1 = (int) (hour0->radius * cos( hour0->angle + PI_RAD *
					hour0->point_numbers[ i + 1 ] * 360.0 /
					hour0->num_point ) ) + y0;
	x2 = (int) (hour0->radius * sin( hour0->angle + PI_RAD *
					hour0->point_numbers[ i + 2 ] * 360.0 /
					hour0->num_point ) ) + x0;
	y2 = (int) (hour0->radius * cos( hour0->angle + PI_RAD *
					hour0->point_numbers[ i + 2 ] * 360.0 /
					hour0->num_point ) ) + y0;
	x3 = (int) (hour0->radius * sin( hour0->angle + PI_RAD *
					hour0->point_numbers[ i + 3 ] * 360.0 /
					hour0->num_point ) ) + x0;
	y3 = (int) (hour0->radius * cos( hour0->angle + PI_RAD *
					hour0->point_numbers[ i + 3 ] * 360.0 /
					hour0->num_point ) ) + y0;
	if ( hour0->draw )
	  {
	     XDrawLine(display, drawable, tclock->gc, x , y , x1 , y1 );
	     XDrawLine(display, drawable, tclock->gc, x1 , y1 , x2 , y2 );
	     XDrawLine(display, drawable, tclock->gc, x2 , y2 , x3 , y3 );
	     XDrawLine(display, drawable, tclock->gc, x , y , x3 , y3 );
	  }
	else
	  {
	     XPoint xy[4];

	     xy[0].x = x;
	     xy[0].y = y;
	     xy[1].x = x1;
	     xy[1].y = y1;
	     xy[2].x = x2;
	     xy[2].y = y2;
	     xy[3].x = x3;
	     xy[3].y = y3;
	     XFillPolygon(display, drawable, tclock->gc, xy, 4 , Convex,
			  CoordModeOrigin);
	  }
     }
}

static void
free_hour(toneclockstruct *tclock)
{
	if (tclock->hour != NULL) {
		free(tclock->hour);
		tclock->hour = (toneclockhour *) NULL;
	}
}

static void
free_toneclock(Display *display, toneclockstruct *tclock)
{
	ModeInfo *mi = tclock->mi;

	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		MI_WHITE_PIXEL(mi) = tclock->whitepixel;
		MI_BLACK_PIXEL(mi) = tclock->blackpixel;
#ifndef STANDALONE
		MI_FG_PIXEL(mi) = tclock->fg;
		MI_BG_PIXEL(mi) = tclock->bg;
#endif
		if (tclock->colors != NULL) {
			if (tclock->ncolors && !tclock->no_colors)
				free_colors(display, tclock->cmap, tclock->colors, tclock->ncolors);
			free(tclock->colors);
			tclock->colors = (XColor *) NULL;
		}
		if (tclock->cmap != None) {
			XFreeColormap(display, tclock->cmap);
			tclock->cmap = None;
		}
	}
	if (tclock->gc != None) {
		XFreeGC(display, tclock->gc);
		tclock->gc = None;
	}
	free_hour(tclock);   
#ifndef NO_DBUF
	if (tclock->dbuf != None) {
		XFreePixmap(display, tclock->dbuf);
		tclock->dbuf = None;
	}
	if (tclock->dbuf_gc != None) {
		XFreeGC(display, tclock->dbuf_gc);
		tclock->dbuf_gc = None;
	}
#endif
}

#ifndef STANDALONE
extern char *background;
extern char *foreground;
#endif

void
init_toneclock(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         i, size_hour, istart;
	toneclockstruct *tclock;

/* initialize */
	if (toneclocks == NULL) {
		if ((toneclocks = (toneclockstruct *) calloc(MI_NUM_SCREENS(mi),
				sizeof (toneclockstruct))) == NULL)
			return;
	}
	tclock = &toneclocks[MI_SCREEN(mi)];
	tclock->mi = mi;

	if (tclock->gc == None) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			tclock->fg = MI_FG_PIXEL(mi);
			tclock->bg = MI_BG_PIXEL(mi);
#endif
			tclock->blackpixel = MI_BLACK_PIXEL(mi);
			tclock->whitepixel = MI_WHITE_PIXEL(mi);
			if ((tclock->cmap = XCreateColormap(display, window,
					MI_VISUAL(mi), AllocNone)) == None) {
				free_toneclock(display, tclock);
				return;
			}
			XSetWindowColormap(display, window, tclock->cmap);
			(void) XParseColor(display, tclock->cmap, "black", &color);
			(void) XAllocColor(display, tclock->cmap, &color);
			MI_BLACK_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, tclock->cmap, "white", &color);
			(void) XAllocColor(display, tclock->cmap, &color);
			MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
			(void) XParseColor(display, tclock->cmap, background, &color);
			(void) XAllocColor(display, tclock->cmap, &color);
			MI_BG_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, tclock->cmap, foreground, &color);
			(void) XAllocColor(display, tclock->cmap, &color);
			MI_FG_PIXEL(mi) = color.pixel;
#endif
			tclock->colors = (XColor *) NULL;
			tclock->ncolors = 0;
		}
		if ((tclock->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None) {
			free_toneclock(display, tclock);
			return;
		}
	}
/* Clear Display */
	MI_CLEARWINDOW(mi);
	tclock->painted = False;
	XSetFunction(display, tclock->gc, GXxor);


/*Set up toneclock data */
	if (MI_IS_FULLRANDOM(mi)) {
	   if (NRAND(10))
	     tclock->original = False;
	   else
	     tclock->original = True;
	} else {
	   tclock->original = original;
	}
	tclock->direction = (LRAND() & 1) ? 1 : -1;
	tclock->win_width = MI_WIDTH(mi);
	tclock->win_height = MI_HEIGHT(mi);
	if (tclock->hour != NULL)
		free_hour(tclock);
	if ( tclock->original )
          {
	     tclock->num_hour = 12;
	  }
        else
          {
	     tclock->num_hour = MI_COUNT(mi);
	  }
        tclock->x0 = tclock->win_width / 2;
        tclock->y0 = tclock->win_height / 2;
	if (tclock->num_hour == 0) {
		tclock->num_hour = DEF_NUM_hour;
	} else if (tclock->num_hour < 0) {
		tclock->num_hour = NRAND(-tclock->num_hour) + 1;
	}
        if ( tclock->num_hour < 12 )
          istart = NRAND( 12 - tclock->num_hour );
        else
          istart = 0;
	if ((tclock->hour = (toneclockhour *) calloc(tclock->num_hour,
			sizeof (toneclockhour))) == NULL) {
		free_toneclock(display, tclock);
		return;
	}
	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
/* Set up colour map */
		if (tclock->colors != NULL) {
			if (tclock->ncolors && !tclock->no_colors)
				free_colors(display, tclock->cmap, tclock->colors, tclock->ncolors);
			free(tclock->colors);
			tclock->colors = (XColor *) NULL;
		}
		tclock->ncolors = MI_NCOLORS(mi);
		if (tclock->ncolors < 2)
			tclock->ncolors = 2;
		if (tclock->ncolors <= 2)
			tclock->mono_p = True;
		else
			tclock->mono_p = False;

		if (tclock->mono_p)
			tclock->colors = (XColor *) NULL;
		else
			if ((tclock->colors = (XColor *) malloc(sizeof (*tclock->colors) *
					(tclock->ncolors + 1))) == NULL) {
				free_toneclock(display, tclock);
				return;
			}
		tclock->cycle_p = has_writable_cells(mi);
		if (tclock->cycle_p) {
			if (MI_IS_FULLRANDOM(mi)) {
				if (!NRAND(8))
					tclock->cycle_p = False;
				else
					tclock->cycle_p = True;
			} else {
				tclock->cycle_p = cycle_p;
			}
		}
		if (!tclock->mono_p) {
			if (!(LRAND() % 10))
				make_random_colormap(
#ifdef STANDALONE
						MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
						tclock->cmap, tclock->colors, &tclock->ncolors,
						True, True, &tclock->cycle_p);
			else if (!(LRAND() % 2))
				make_uniform_colormap(
#ifdef STANDALONE
						MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
                  tclock->cmap, tclock->colors, &tclock->ncolors,
						      True, &tclock->cycle_p);
			else
				make_smooth_colormap(
#ifdef STANDALONE
						MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
                 tclock->cmap, tclock->colors, &tclock->ncolors,
						     True, &tclock->cycle_p);
		}
		XInstallColormap(display, tclock->cmap);
		if (tclock->ncolors < 2) {
			tclock->ncolors = 2;
			tclock->no_colors = True;
		} else
			tclock->no_colors = False;
		if (tclock->ncolors <= 2)
			tclock->mono_p = True;

		if (tclock->mono_p)
			tclock->cycle_p = False;

	}
#ifndef NO_DBUF
	if (tclock->dbuf != None)
		XFreePixmap(display, tclock->dbuf);
	        tclock->dbuf = XCreatePixmap(display, window,
			tclock->win_width,
			tclock->win_height,
			MI_DEPTH(mi));
	/* Allocation checked */
	if (tclock->dbuf != None) {
		XGCValues   gcv;

		gcv.foreground = 0;
		gcv.background = 0;
		gcv.graphics_exposures = False;
		gcv.function = GXcopy;

		if (tclock->dbuf_gc != None)
			XFreeGC(display, tclock->dbuf_gc);
		if ((tclock->dbuf_gc = XCreateGC(display, tclock->dbuf,
			GCForeground | GCBackground | GCGraphicsExposures | GCFunction,
				&gcv)) == None) {
			XFreePixmap(display, tclock->dbuf);
			tclock->dbuf = None;
		} else {
			XFillRectangle(display, tclock->dbuf, tclock->dbuf_gc,
				0, 0, tclock->win_width, tclock->win_height);
			XSetBackground(display, MI_GC(mi), MI_BLACK_PIXEL(mi));
			XSetFunction(display, MI_GC(mi), GXcopy);
		}
	}
#endif
	tclock->angle = NRAND(360) * PI_RAD;
	tclock->velocity = (NRAND(7) - 3) * PI_RAD;
	size_hour = min( tclock->win_width , tclock->win_height) / 3;
   tclock->pulsating = False;
   tclock->moving = False;
   tclock->anglex = 0.0;
   tclock->angley = 0.0;
   tclock->fill = 0;
   tclock->radius = size_hour;
   tclock->max_radius =0.0;
   if ( ( !tclock->original && NRAND( 15 ) == 3 ) || tclock->num_hour > 12 )
     tclock->randomhour = True;
   else
     tclock->randomhour = False;
   if ( !tclock->original && tclock->win_width > 20 )
     {
	if ( abs( MI_SIZE(mi) ) > size_hour ) {
	   if ( MI_SIZE( mi ) < 0 )
	     {
		size_hour = -size_hour;
	     }
	}
	else
	  {
	     size_hour = MI_SIZE(mi);
          }
    	if ( size_hour < 0 )
          {
	     tclock->radius = MIN(NRAND( size_hour - 10) + 10,
		tclock->radius );
          }
        else
          {
  	     tclock->radius = MIN( size_hour , tclock->radius );
          }
	if ( MI_IS_FULLRANDOM( mi ) )
	  {
	     if ( NRAND(2) )
	       tclock->pulsating = True;
	     else
	       tclock->pulsating = False;
	     tclock->fill = NRAND( 101 );
	  }
	else
	  {
	     tclock->pulsating = pulsating;
	     tclock->fill = fill;
	  }
     }
   tclock->phase = 0.0;
   if ( tclock->pulsating )
	tclock->ph_vel = (NRAND(7) - 3) * PI_RAD;
   for (i = 0; i < tclock->num_hour; i++) {
		toneclockhour *hour0;

		hour0 = &tclock->hour[i];
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			if (tclock->ncolors > 2)
				hour0->colour = NRAND(tclock->ncolors - 2) + 2;
			else
				hour0->colour = 1;	/* Just in case */
			XSetForeground(display, tclock->gc, tclock->colors[hour0->colour].pixel);
		} else {
			if (MI_NPIXELS(mi) > 2)
				hour0->colour = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			else
				hour0->colour = 1;	/*Xor'red so WHITE may not be appropriate */
			XSetForeground(display, tclock->gc, hour0->colour);
		}
		hour0->angle = NRAND(360) * PI_RAD;
		hour0->velocity = (NRAND(7) - 3) * PI_RAD;
	        hour0->radius = tclock->radius / 5.0;
      		tclock->max_radius = MAX( tclock->max_radius , hour0->radius );
	        hour0->num_point = 12;
		hour0->num_point1 = 16;
                if ( tclock->randomhour )
	          {
		     int j;

		     hour0->point_numbers = tclock->hexadecimal_clock + i *
			     hour0->num_point1;
		     if ( NRAND( 14 ) == 4 )
		       {
			  for (j = 0; j < ( hour0->num_point1 / 4 ) - 1 ; j++)
			    {
			       hour0->point_numbers[ j * 4 ] = NRAND( 12 ) + 1;
			       hour0->point_numbers[ j * 4 + 1 ] = NRAND( 12 )
				 + 1;
			       hour0->point_numbers[ j * 4 + 2 ] = NRAND( 12 )
				 + 1;
			       hour0->point_numbers[ j * 4 + 3 ] = NRAND( 12 )
				 + 1;
			    }
			  hour0->point_numbers[ hour0->num_point1 / 4 ] = 1;
			  hour0->point_numbers[ 1 + hour0->num_point1 / 4 ] =
			    1;
			  hour0->point_numbers[ 2 + hour0->num_point1 / 4 ] =
			    1;
			  hour0->point_numbers[ 3 + hour0->num_point1 / 4 ] =
			    1;
		       }
		     else
		       {
			  for (j = 0; j < hour0->num_point1 / 4 ; j++)
			    {
			       hour0->point_numbers[ j * 4 ] = NRAND( 12 ) + 1;
			       hour0->point_numbers[ j * 4 + 1 ] =
				 hour0->point_numbers[ j * 4 ];
			       hour0->point_numbers[ j * 4 + 2 ] = NRAND( 12 )
				 + 1;
			       hour0->point_numbers[ j * 4 + 3 ] = NRAND( 12 )
				 + 1;
			    }
		       }
	          }
                else
	          hour0->point_numbers = original_clock[i+istart];
		if ( NRAND( 100 ) >= tclock->fill )
			hour0->draw = True;
		else
			hour0->draw = False;
#ifdef NO_DBUF
		{
		  int x0 , y0;

		  x0 = (int) (tclock->radius * sin( -tclock->angle - PI_RAD * i *
			360.0 / tclock->num_hour ) *
			0.5 * ( 1 + cos( tclock->phase ) ) + tclock->x0 +
			tclock->a_x * sin( tclock->anglex ) );
		  y0 = (int) (tclock->radius * cos( -tclock->angle - PI_RAD * i *
			360.0 / tclock->num_hour ) *
			0.5 * ( 1 + cos( tclock->phase ) ) + tclock->y0 +
			tclock->a_y * sin( tclock->angley ) );
	 	  toneclock_drawhour(mi , hour0 , x0 , y0 );
		}
#endif
	}
   tclock->a_x = 0;
   tclock->a_y = 0;
   if ( !tclock->original && tclock->win_width > 20 )
     {
	if ( tclock->radius < min( tclock->win_width , tclock->win_height) /
	     4 )
	  {
	     if ( MI_IS_FULLRANDOM( mi ) )
	       {
		  if ( NRAND(2) )
		    tclock->moving = True;
	       }
	     else
	       {
		    tclock->moving = move_clock;
	       }
	     if ( tclock->moving )
	       {
		  tclock->a_x = floor( ( tclock->win_width / 2 ) - 1.05 *
					( tclock->radius + tclock->max_radius )
					);
		  tclock->a_y = floor( ( tclock->win_height / 2 ) - 1.05 *
					( tclock->radius + tclock->max_radius )
					);
		  tclock->vx = (NRAND(15) - 7) * PI_RAD;
		  tclock->vy = (NRAND(15) - 7) * PI_RAD;
	       }
	  }
     }
	XFlush(display);
	XSetFunction(display, tclock->gc, GXcopy);
}

void
draw_toneclock(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         i;
	toneclockstruct *tclock;

	if (toneclocks == NULL)
		return;
	tclock = &toneclocks[MI_SCREEN(mi)];
	if (tclock->hour == NULL)
		return;

	if (tclock->no_colors) {
		free_toneclock(display, tclock);
		init_toneclock(mi);
		return;
	}
	XSetFunction(display, tclock->gc, GXxor);
#ifndef NO_DBUF
        XSetForeground(display,tclock->dbuf_gc,MI_BLACK_PIXEL(mi));
	XFillRectangle(display, tclock->dbuf, tclock->dbuf_gc,
		0, 0, tclock->win_width, tclock->win_height);
#endif
	tclock->painted = True;
	MI_IS_DRAWN(mi) = True;
/* Rotate colours */
	if (tclock->cycle_p) {
		rotate_colors(display, tclock->cmap, tclock->colors, tclock->ncolors,
			      tclock->direction);
		if (!(LRAND() % 1000))
			tclock->direction = -tclock->direction;
	}
	for (i = 0; i < tclock->num_hour; i++) {
		toneclockhour *hour0;
	   int x0 , y0;

	   x0 = (int) (tclock->radius * sin( -tclock->angle - PI_RAD * i *
					     360.0 / tclock->num_hour ) *
	     0.5 * ( 1 + cos( tclock->phase ) ) + tclock->x0 +
		       tclock->a_x * sin( tclock->anglex ) );
	   y0 = (int) (tclock->radius * cos( -tclock->angle - PI_RAD * i *
					     360.0 / tclock->num_hour ) *
	       0.5 * ( 1 + cos( tclock->phase ) ) + tclock->y0 +
		       tclock->a_y * sin( tclock->angley ) );
		hour0 = &tclock->hour[i];
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XSetForeground(display, tclock->gc, tclock->colors[hour0->colour].pixel);
		} else {
			XSetForeground(display, tclock->gc, hour0->colour);
		}
		toneclock_drawhour(mi, hour0 , x0 , y0);
		hour0->velocity += ((float) NRAND(1001) - 500.0) / 200000.0;
		hour0->angle += hour0->velocity;
	}
   tclock->velocity += ((float) NRAND(1001) - 500.0) / 200000.0;
   tclock->angle += tclock->velocity;
	       if ( tclock->pulsating )
	       tclock->phase += tclock->ph_vel;
   if ( tclock->moving )
     {
	tclock->anglex += tclock->vx;
	tclock->angley += tclock->vy;
     }
#ifdef NO_DBUF
	for (i = 0; i < tclock->num_hour; i++) {
		toneclockhour *hour0;
	   int x0 , y0;

	   x0 = (int) (tclock->radius * sin( -tclock->angle - PI_RAD * i *
					     360.0 / tclock->num_hour ) *
	     0.5 * ( 1 + cos( tclock->phase ) ) + tclock->x0 +
		       tclock->a_x * sin( tclock->anglex ) );
	   y0 = (int) (tclock->radius * cos( -tclock->angle - PI_RAD * i *
					     360.0 / tclock->num_hour ) *
	       0.5 * ( 1 + cos( tclock->phase ) ) + tclock->y0 +
		       tclock->a_y * sin( tclock->angley ) );
		hour0 = &tclock->hour[i];
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XSetForeground(display, tclock->gc, tclock->colors[hour0->colour].pixel);
		} else {
			XSetForeground(display, tclock->gc, hour0->colour);
		}
		toneclock_drawhour(mi, hour0 , x0 , y0);
	}
#else
     XFlush(display);
     XCopyArea (display, tclock->dbuf, window, tclock->dbuf_gc, 0, 0,
		     tclock->win_width, tclock->win_height, 0, 0);
     XFlush(display);
     XSetForeground(display,tclock->dbuf_gc,MI_BLACK_PIXEL(mi));
     XFillRectangle(display, tclock->dbuf, tclock->dbuf_gc,
		0, 0, tclock->win_width, tclock->win_height);
#endif
	XSetFunction(display, tclock->gc, GXcopy);
}

void
refresh_toneclock(ModeInfo * mi)
{
	toneclockstruct *tclock;

	if (toneclocks == NULL)
		return;
	tclock = &toneclocks[MI_SCREEN(mi)];
	if (tclock->hour == NULL)
		return;

	if (!tclock->painted)
		return;
	MI_CLEARWINDOW(mi);
#ifdef NO_DBUF
	{
	  Display    *display = MI_DISPLAY(mi);
	  int i;

	  XSetFunction(display, tclock->gc, GXxor);
	  for (i = 0; i < tclock->num_hour; i++) {
		toneclockhour *hour0;
	   int x0 , y0;

	   x0 = (int) (tclock->radius * sin( -tclock->angle - PI_RAD * i *
					     360.0 / tclock->num_hour ) +
	     tclock->x0 + tclock->a_x * sin( tclock->anglex ) );
	   y0 = (int) (tclock->radius * cos( -tclock->angle - PI_RAD * i *
					     360.0 / tclock->num_hour ) +
	     tclock->y0 + tclock->a_y * sin( tclock->angley ) );
		hour0 = &tclock->hour[i];
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XSetForeground(display, tclock->gc, tclock->colors[hour0->colour].pixel);
		} else {
			XSetForeground(display, tclock->gc, hour0->colour);
		}
		toneclock_drawhour(mi, hour0 , x0 , y0 );
	   }
	  XSetFunction(display, tclock->gc, GXcopy);
	}
#endif
}

void
release_toneclock(ModeInfo * mi)
{
	if (toneclocks != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_toneclock(MI_DISPLAY(mi), &toneclocks[screen]);
		free(toneclocks);
		toneclocks = (toneclockstruct *) NULL;
	}
}

#endif /* MODE_toneclock */
