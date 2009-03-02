/* -*- Mode: C; tab-width: 4 -*- */
/* tik_tak --- Rotating objects inspired by the Belgium television program *
 * TIKTAK */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)tik_tak.c	5.00 2000/11/01 xlockmore";

#endif

/*-
 * Copyright (c) 1999 by Jouk Jansen <joukj@hrem.stm.tudelft.nl>
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
 * A rotating polygon-mode. This mode is inspired (but different) by some
 * parts of the Belgium television program, TIKTAK, intended for babies.
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 16-Sep-1999: Added shells and stars
 * 08-Sep-1999: Created
 *
 * TODO list :
 *   -make the objects move hormonically around the centre
 *   -Create run-time options
 */

#ifdef STANDALONE
#define MODE_tik_tak
#define PROGCLASS "Tik_Tak"
#define HACK_INIT init_tik_tak
#define HACK_DRAW draw_tik_tak
#define tik_tak_opts xlockmore_opts
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

#ifdef MODE_tik_tak

#define DEF_NUM_OBJECT 4

#define DEF_CYCLE "True"

static Bool cycle_p;

static XrmOptionDescRec opts[] =
{
	{(char *) "-cycle", (char *) ".tik_tak.cycle", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+cycle", (char *) ".tik_tak.cycle", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(void *) & cycle_p, (char *) "cycle", (char *) "Cycle", (char *) DEF_CYCLE, t_Bool}
};
static OptionStruct desc[] =
{
	{(char *) "-/+cycle", (char *) "turn on/off colour cycling"}
};

ModeSpecOpt tik_tak_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   tik_tak_description =
{"tik_tak", "init_tik_tak", "draw_tik_tak", "release_tik_tak",
 "refresh_tik_tak", "init_tik_tak", (char *) NULL, &tik_tak_opts,
 60000, -20, 200, -1000, 64, 1.0, "",
 "Shows rotating polygons", 0, NULL};

#endif

#define PI_RAD (M_PI / 180.0)

typedef struct {
	unsigned long colour;
        Bool        inner;
	float       angle, velocity_a, angle1, velocity_a1;
	float       size_mult, size_mult1;
	int         num_point, size_ob;
	int         num_point1, size_ob1;
	XPoint      *xy , *xy1;
} tik_takobject;

typedef struct {
	Bool        painted;
	int         win_width, win_height, num_object, x0, y0;
	tik_takobject *object;
	GC          gc;
	Colormap    cmap;
	XColor     *colors;
	int         ncolors;
	Bool        cycle_p, mono_p, no_colors;
	unsigned long blackpixel, whitepixel, fg, bg;
	int         direction;
	ModeInfo   *mi;
} tik_takstruct;

static tik_takstruct *tik_taks = (tik_takstruct *) NULL;

static void
tik_tak_setupobject( ModeInfo * mi , tik_takobject * object0)
{
	tik_takstruct *tiktak = &tik_taks[MI_SCREEN(mi)];
	int         x_0=tiktak->x0 , y_0=tiktak->y0 , i;
   float curangle;

   for (i = 0; i <= object0->num_point; i++)
     {
	object0->xy[i].x = object0->xy[i+object0->num_point+1].x;
	object0->xy[i].y = object0->xy[i+object0->num_point+1].y;
     }
   for (i = 0; i < object0->num_point; i++)
     {
	object0->size_ob = (int) (object0->size_ob * object0->size_mult);
	object0->size_mult = 1.0 / object0->size_mult;
	curangle = object0->angle + i * 360.0 * PI_RAD /
	  (float) object0->num_point;
	object0->xy[i+object0->num_point+1].x = x_0 +
	  (int) (object0->size_ob * cos(curangle)) +
	  (int) (object0->size_ob * sin(curangle));
	object0->xy[i+object0->num_point+1].y = y_0 +
	  (int) (object0->size_ob * cos(curangle)) -
	  (int) (object0->size_ob * sin(curangle));
     }
   object0->xy[object0->num_point*2+1].x = object0->xy[object0->num_point+1].x;
   object0->xy[object0->num_point*2+1].y = object0->xy[object0->num_point+1].y;

   if ( object0->inner )
     {
	for (i = 0; i <= object0->num_point1; i++)
	  {
	     object0->xy1[i].x = object0->xy1[i+object0->num_point1+1].x;
	     object0->xy1[i].y = object0->xy1[i+object0->num_point1+1].y;
	  }

	for (i = 0; i < object0->num_point1; i++)
	  {
	     object0->size_ob1 = (int) (object0->size_ob1 * object0->size_mult1);
	     object0->size_mult1 = 1.0 / object0->size_mult1;
	     curangle = object0->angle1 + i * 360.0 * PI_RAD /
	       (float) object0->num_point1;
	     object0->xy1[i+object0->num_point1+1].x = x_0 +
	       (int) (object0->size_ob1 * cos(curangle)) +
	       (int) (object0->size_ob1 * sin(curangle));
	     object0->xy1[i+object0->num_point1+1].y = y_0 +
	       (int) (object0->size_ob1 * cos(curangle)) -
	       (int) (object0->size_ob1 * sin(curangle));
	  }
	object0->xy1[object0->num_point1*2+1].x =
	  object0->xy1[object0->num_point1+1].x;
	object0->xy1[object0->num_point1*2+1].y =
	  object0->xy1[object0->num_point1+1].y;
     }
}

static void
tik_tak_reset_object( tik_takobject * object0)
{
   int i;

   for (i = 0; i <= object0->num_point; i++)
     {
	object0->xy[i].x = object0->xy[object0->num_point+1].x;
	object0->xy[i].y = object0->xy[object0->num_point+1].y;
     }

   if ( object0->inner )
     {
	for (i = 0; i <= object0->num_point1; i++)
	  {
	     object0->xy1[i].x = object0->xy1[object0->num_point1+1].x;
	     object0->xy1[i].y = object0->xy1[object0->num_point1+1].y;
	  }
     }
}

static void
tik_tak_drawobject(ModeInfo * mi, tik_takobject * object0 )
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	tik_takstruct *tiktak = &tik_taks[MI_SCREEN(mi)];

   XFillPolygon(display, window, tiktak->gc, object0->xy ,
		object0->num_point * 2 + 2 , Complex , CoordModeOrigin);
   if ( object0->inner )
     XFillPolygon(display, window, tiktak->gc, object0->xy1 ,
		  object0->num_point1 * 2 + 2 , Complex , CoordModeOrigin);
}

static void
free_tik_tak(Display *display, tik_takstruct *tiktak)
{
	ModeInfo *mi = tiktak->mi;

	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		MI_WHITE_PIXEL(mi) = tiktak->whitepixel;
		MI_BLACK_PIXEL(mi) = tiktak->blackpixel;
#ifndef STANDALONE
		MI_FG_PIXEL(mi) = tiktak->fg;
		MI_BG_PIXEL(mi) = tiktak->bg;
#endif
		if (tiktak->colors != NULL) {
			if (tiktak->ncolors && !tiktak->no_colors)
				free_colors(display, tiktak->cmap, tiktak->colors, tiktak->ncolors);
			free(tiktak->colors);
			tiktak->colors = (XColor *) NULL;
		}
		if (tiktak->cmap != None) {
			XFreeColormap(display, tiktak->cmap);
			tiktak->cmap = None;
		}
	}
	if (tiktak->gc != None) {
		XFreeGC(display, tiktak->gc);
		tiktak->gc = None;
	}
	if (tiktak->object != NULL) {
		int i;

		for (i = 0; i < tiktak->num_object; i++) {
			tik_takobject *object0;

			object0 = &tiktak->object[i];
		 	if (object0->xy1 != NULL)
				free(object0->xy1);
			if (object0->xy != NULL)
				free(object0->xy);
		}
		free(tiktak->object);
		tiktak->object = (tik_takobject *) NULL;
	 }
}

#ifndef STANDALONE
extern char *background;
extern char *foreground;
#endif

void
init_tik_tak(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         i, max_objects, size_object;
	tik_takstruct *tiktak;

/* initialize */
	if (tik_taks == NULL) {
		if ((tik_taks = (tik_takstruct *) calloc(MI_NUM_SCREENS(mi),
				sizeof (tik_takstruct))) == NULL)
			return;
	}
	tiktak = &tik_taks[MI_SCREEN(mi)];
	tiktak->mi = mi;

	if (tiktak->gc == None) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			tiktak->fg = MI_FG_PIXEL(mi);
			tiktak->bg = MI_BG_PIXEL(mi);
#endif
			tiktak->blackpixel = MI_BLACK_PIXEL(mi);
			tiktak->whitepixel = MI_WHITE_PIXEL(mi);
			if ((tiktak->cmap = XCreateColormap(display, window,
					MI_VISUAL(mi), AllocNone)) == None) {
				free_tik_tak(display, tiktak);
				return;
			}
			XSetWindowColormap(display, window, tiktak->cmap);
			(void) XParseColor(display, tiktak->cmap, "black", &color);
			(void) XAllocColor(display, tiktak->cmap, &color);
			MI_BLACK_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, tiktak->cmap, "white", &color);
			(void) XAllocColor(display, tiktak->cmap, &color);
			MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
			(void) XParseColor(display, tiktak->cmap, background, &color);
			(void) XAllocColor(display, tiktak->cmap, &color);
			MI_BG_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, tiktak->cmap, foreground, &color);
			(void) XAllocColor(display, tiktak->cmap, &color);
			MI_FG_PIXEL(mi) = color.pixel;
#endif
			tiktak->colors = (XColor *) NULL;
			tiktak->ncolors = 0;
		}
		if ((tiktak->gc = XCreateGC(display, MI_WINDOW(mi),
			     (unsigned long) 0, (XGCValues *) NULL)) == None) {
			free_tik_tak(display, tiktak);
			return;
		}
	}
/* Clear Display */
	MI_CLEARWINDOW(mi);
	tiktak->painted = False;
	XSetFunction(display, tiktak->gc, GXxor);


/*Set up tik_tak data */
	tiktak->direction = (LRAND() & 1) ? 1 : -1;
	tiktak->win_width = MI_WIDTH(mi);
	tiktak->win_height = MI_HEIGHT(mi);
	tiktak->num_object = MI_COUNT(mi);
        tiktak->x0 = tiktak->win_width / 2;
        tiktak->y0 = tiktak->win_height / 2;
	max_objects = MI_COUNT(mi);
	if (tiktak->num_object == 0) {
		tiktak->num_object = DEF_NUM_OBJECT;
		max_objects = DEF_NUM_OBJECT;
	} else if (tiktak->num_object < 0) {
		max_objects = -tiktak->num_object;
		tiktak->num_object = NRAND(-tiktak->num_object) + 1;
	}
	if (tiktak->object == NULL)
		if ((tiktak->object = (tik_takobject *) calloc(max_objects,
				sizeof (tik_takobject))) == NULL) {
			free_tik_tak(display, tiktak);
			return;
		}
	size_object = MIN( tiktak->win_width , tiktak->win_height) / 3;
	if ( abs( MI_SIZE(mi) ) > size_object) {
	   if ( MI_SIZE( mi ) < 0 )
	     {
		size_object = -size_object;
	     }
	}
   else
     {
	size_object = MI_SIZE(mi);
     }
	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
/* Set up colour map */
		if (tiktak->colors != NULL) {
			if (tiktak->ncolors && !tiktak->no_colors)
				free_colors(display, tiktak->cmap, tiktak->colors, tiktak->ncolors);
			free(tiktak->colors);
			tiktak->colors = (XColor *) NULL;
		}
		tiktak->ncolors = MI_NCOLORS(mi);
		if (tiktak->ncolors < 2)
			tiktak->ncolors = 2;
		if (tiktak->ncolors <= 2)
			tiktak->mono_p = True;
		else
			tiktak->mono_p = False;

		if (tiktak->mono_p)
			tiktak->colors = (XColor *) NULL;
		else
			if ((tiktak->colors = (XColor *) malloc(sizeof (*tiktak->colors) *
					(tiktak->ncolors + 1))) == NULL) {
				free_tik_tak(display, tiktak);
				return;
			}
		tiktak->cycle_p = has_writable_cells(mi);
		if (tiktak->cycle_p) {
			if (MI_IS_FULLRANDOM(mi)) {
				if (!NRAND(8))
					tiktak->cycle_p = False;
				else
					tiktak->cycle_p = True;
			} else {
				tiktak->cycle_p = cycle_p;
			}
		}
		if (!tiktak->mono_p) {
			if (!(LRAND() % 10))
				make_random_colormap(
#ifdef STANDALONE
						MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
						tiktak->cmap, tiktak->colors, &tiktak->ncolors,
						True, True, &tiktak->cycle_p);
			else if (!(LRAND() % 2))
				make_uniform_colormap(
#ifdef STANDALONE
						MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
                  tiktak->cmap, tiktak->colors, &tiktak->ncolors,
						      True, &tiktak->cycle_p);
			else
				make_smooth_colormap(
#ifdef STANDALONE
						MI_DISPLAY(mi), MI_WINDOW(mi),
#else
            mi,
#endif
                 tiktak->cmap, tiktak->colors, &tiktak->ncolors,
						     True, &tiktak->cycle_p);
		}
		XInstallColormap(display, tiktak->cmap);
		if (tiktak->ncolors < 2) {
			tiktak->ncolors = 2;
			tiktak->no_colors = True;
		} else
			tiktak->no_colors = False;
		if (tiktak->ncolors <= 2)
			tiktak->mono_p = True;

		if (tiktak->mono_p)
			tiktak->cycle_p = False;

	}
	for (i = 0; i < tiktak->num_object; i++) {
		tik_takobject *object0;

		object0 = &tiktak->object[i];
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			if (tiktak->ncolors > 2)
				object0->colour = NRAND(tiktak->ncolors - 2) + 2;
			else
				object0->colour = 1;	/* Just in case */
			XSetForeground(display, tiktak->gc, tiktak->colors[object0->colour].pixel);
		} else {
			if (MI_NPIXELS(mi) > 2)
				object0->colour = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
			else
				object0->colour = 1;	/*Xor'red so WHITE may not be appropriate */
			XSetForeground(display, tiktak->gc, object0->colour);
		}
		object0->angle = NRAND(90) * PI_RAD;
		object0->angle1 = NRAND(90) * PI_RAD;
		object0->velocity_a = (NRAND(7) - 3) * PI_RAD;
		object0->velocity_a1 = (NRAND(7) - 3) * PI_RAD;
		if (size_object == 0)
			object0->size_ob = 9;
		else if (size_object > 0)
			object0->size_ob = size_object;
		else
			object0->size_ob = NRAND(-size_object) + 1;
		object0->size_ob++;
		object0->num_point = NRAND(6)+3;
	   if (LRAND() & 1)
	     object0->size_mult = 1.0;
	   else
	     {
		object0->num_point *= 2;
		object0->size_mult = 1.0 - ( 1.0 / (float) ((LRAND() & 1) +
							    2 ) );
	     }
	   if (object0->xy != NULL)
			free(object0->xy);
	   if ((object0->xy = (XPoint *) malloc(sizeof( XPoint ) *
				(2 * object0->num_point + 2))) == NULL) {
			free_tik_tak(display, tiktak);
			return;
		}
	   if ((LRAND() & 1) || object0->size_ob < 10 )
	     {
		object0->inner = False;
		if ( object0->xy1 != NULL ) free( object0->xy1 );
		object0->xy1 = (XPoint *) NULL;
	     }
	   else
	     {
		object0->inner = True;
	        object0->size_ob1 = object0->size_ob -
		  NRAND( object0->size_ob / 5 ) - 1;
		object0->num_point1 = NRAND(6)+3;
		if (LRAND() & 1)
		  object0->size_mult1 = 1.0;
		else
		  {
		     object0->num_point1 *= 2;
		     object0->size_mult1 = 1.0 -
		        ( 1.0 / (float) ((LRAND() & 1) + 2 ) );
		  }
		if (object0->xy1 != NULL)
			free(object0->xy1);
		if ((object0->xy1 = (XPoint *) malloc(sizeof( XPoint ) *
				(2 * object0->num_point1 + 2))) == NULL) {
			free_tik_tak(display, tiktak);
			return;
		}
		object0->size_mult1 = 1.0;
	     }
		tik_tak_setupobject( mi , object0);
		tik_tak_reset_object( object0);
		tik_tak_drawobject(mi, object0 );
	}
	XFlush(display);
	XSetFunction(display, tiktak->gc, GXcopy);
}

void
draw_tik_tak(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	int         i;
	tik_takstruct *tiktak;

	if (tik_taks == NULL)
		return;
	tiktak = &tik_taks[MI_SCREEN(mi)];
	if (tiktak->object == NULL)
		return;

	if (tiktak->no_colors) {
		free_tik_tak(display, tiktak);
		init_tik_tak(mi);
		return;
	}
	tiktak->painted = True;
	MI_IS_DRAWN(mi) = True;
	XSetFunction(display, tiktak->gc, GXxor);

/* Rotate colours */
	if (tiktak->cycle_p) {
		rotate_colors(display, tiktak->cmap, tiktak->colors, tiktak->ncolors,
			      tiktak->direction);
		if (!(LRAND() % 1000))
			tiktak->direction = -tiktak->direction;
	}
	for (i = 0; i < tiktak->num_object; i++) {
		tik_takobject *object0;

		object0 = &tiktak->object[i];
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XSetForeground(display, tiktak->gc, tiktak->colors[object0->colour].pixel);
		} else {
			XSetForeground(display, tiktak->gc, object0->colour);
		}
		object0->velocity_a += ((float) NRAND(1001) - 500.0) / 200000.0;
		object0->angle += object0->velocity_a;
		object0->velocity_a1 += ((float) NRAND(1001) - 500.0) / 200000.0;
		object0->angle1 += object0->velocity_a1;
		tik_tak_setupobject( mi , object0);
		tik_tak_drawobject(mi, object0 );
	}
	XSetFunction(display, tiktak->gc, GXcopy);
}

void
refresh_tik_tak(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	int         i;
	tik_takstruct *tiktak;

	if (tik_taks == NULL)
		return;
	tiktak = &tik_taks[MI_SCREEN(mi)];
	if (tiktak->object == NULL)
		return;

	if (!tiktak->painted)
		return;
	MI_CLEARWINDOW(mi);
	XSetFunction(display, tiktak->gc, GXxor);

	for (i = 0; i < tiktak->num_object; i++) {
		tik_takobject *object0;

		object0 = &tiktak->object[i];
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XSetForeground(display, tiktak->gc, tiktak->colors[object0->colour].pixel);
		} else {
			XSetForeground(display, tiktak->gc, object0->colour);
		}
		tik_tak_setupobject( mi , object0);
		tik_tak_reset_object( object0);
		tik_tak_drawobject(mi, object0 );
	}
	XSetFunction(display, tiktak->gc, GXcopy);
}

void
release_tik_tak(ModeInfo * mi)
{
	if (tik_taks != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_tik_tak(MI_DISPLAY(mi), &tik_taks[screen]);
		free(tik_taks);
		tik_taks = (tik_takstruct *) NULL;
	}
}

#endif /* MODE_tik_tak */
