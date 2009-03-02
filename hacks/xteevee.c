/*****************************************************************************
 *                                                                           *
 * xteevee -- TV good... TV good...                                          *
 *                                                                           *
 * Copyright (c) 1999 Greg Knauss (greg@eod.com)                             *
 *                                                                           *
 * Permission to use, copy, modify, distribute, and sell this software and   *
 * its documentation for any purpose is hereby granted without fee, provided *
 * that the above copyright notice appear in all copies and that both that   *
 * copyright notice and this permission notice appear in supporting          *
 * documentation.  No representations are made about the suitability of this *
 * software for any purpose.  It is provided "as is" without express or      *
 * implied warranty.                                                         *
 *                                                                           *
 *****************************************************************************/


/* Changelog *****************************************************************

	1.0.0	19991119	Initial release

*/


/* Includes ******************************************************************/
#include "screenhack.h"
#include <X11/Xutil.h>


/* Defines *******************************************************************/
#define XTEEVEE_NAME                    "XTeeVee"
#define XTEEVEE_MODE_EXCLUDE            0
#define XTEEVEE_MODE_INCLUDE_IMPLICIT   1
#define XTEEVEE_MODE_INCLUDE_EXPLICIT   2
#define XTEEVEE_ARG_STATIC              "static"
#define XTEEVEE_ARG_STATIC_SIGNAL       "staticSignal"
#define XTEEVEE_ARG_ROLL                "roll"
#define XTEEVEE_ARG_BARS                "bars"
#define XTEEVEE_ARG_CYCLE               "cycle"
#define XTEEVEE_ARG_DELAY_MODE          "delayMode"
#define XTEEVEE_ARG_DELAY_BETWEEN       "delayBetween"
#define XTEEVEE_STATIC_COLOR_COUNT      6
#define XTEEVEE_STATIC_TILE_COUNT       16
#define XTEEVEE_BARS_COLOR_TOP_COUNT    7
#define XTEEVEE_BARS_COLOR_BOTTOM_COUNT 5


/* Globals *******************************************************************/
char *progclass  = XTEEVEE_NAME;
char *defaults[] =
{
	"*" XTEEVEE_ARG_STATIC        ": true",
	"*" XTEEVEE_ARG_STATIC_SIGNAL ": 50",
	"*" XTEEVEE_ARG_ROLL          ": true",
	"*" XTEEVEE_ARG_BARS          ": true",
	"*" XTEEVEE_ARG_CYCLE         ": true",
	"*" XTEEVEE_ARG_DELAY_MODE    ": 30",
	"*" XTEEVEE_ARG_DELAY_BETWEEN ": 3",
	NULL
};
XrmOptionDescRec options[] =
{
 { "-"    XTEEVEE_ARG_STATIC,"._" XTEEVEE_ARG_STATIC,XrmoptionNoArg,"true" },
 { "-no-" XTEEVEE_ARG_STATIC,"."  XTEEVEE_ARG_STATIC,XrmoptionNoArg,"false" },
 { "-"    XTEEVEE_ARG_ROLL  ,"._" XTEEVEE_ARG_ROLL  ,XrmoptionNoArg,"true" },
 { "-no-" XTEEVEE_ARG_ROLL  ,"."  XTEEVEE_ARG_ROLL  ,XrmoptionNoArg,"false" },
 { "-"    XTEEVEE_ARG_BARS  ,"._" XTEEVEE_ARG_BARS  ,XrmoptionNoArg,"true" },
 { "-no-" XTEEVEE_ARG_BARS  ,"."  XTEEVEE_ARG_BARS  ,XrmoptionNoArg,"false" },
 { "-"    XTEEVEE_ARG_CYCLE ,"."  XTEEVEE_ARG_CYCLE ,XrmoptionNoArg,"true" },
 { "-no-" XTEEVEE_ARG_CYCLE ,"."  XTEEVEE_ARG_CYCLE ,XrmoptionNoArg,"false" },
 { NULL                     ,NULL                   ,0             ,NULL }
};


/* Functions *****************************************************************/

/* Get the forground pixel ================================================= */
void xteevee_FreeColorForeground(Display* x_Disp,XWindowAttributes* x_WinAttr,
      GC x_Gc)
{
	XGCValues x_GcVal;

	if (XGetGCValues(x_Disp,x_Gc,GCForeground,&x_GcVal) != 0)
	{
		XFreeColors(x_Disp,x_WinAttr->colormap,&x_GcVal.foreground,1,
		 0);
	}
}

/* Static ================================================================== */
void xteevee_Static(Display* x_Disp,Window x_Win,XWindowAttributes* x_WinAttr,
      time_t hack_Time,Pixmap hack_Pm)
{
	GC        x_Gc[XTEEVEE_STATIC_COLOR_COUNT];
        unsigned long pixels[XTEEVEE_STATIC_COLOR_COUNT];
        XImage   *xim = 0;
        char     *orig_bits = 0;
	XGCValues x_GcVal;
	int       signal_Strength;
	XColor    color_Color;
	int       color_Index;
	int       tile_Index;
	Pixmap    tile_Tile[XTEEVEE_STATIC_TILE_COUNT];
	int       tile_X;
	int       tile_Y;
	int       tile_Width;
	int       tile_Height;
	char      tile_Used[XTEEVEE_STATIC_TILE_COUNT/2+1];
	int       tile_Selected;

	/* Get any extra arguments */
	signal_Strength = get_integer_resource(XTEEVEE_ARG_STATIC_SIGNAL,
	 "Integer");

	/* Build the GCs */
	color_Color.flags = DoRed|DoGreen|DoBlue;
	for (color_Index = 0;color_Index < XTEEVEE_STATIC_COLOR_COUNT;
	 color_Index++)
	{
		color_Color.red = color_Color.green = color_Color.blue =
		 (((double)color_Index+1)/XTEEVEE_STATIC_COLOR_COUNT)*65535;
		if (XAllocColor(x_Disp,x_WinAttr->colormap,&color_Color) == 0)
		{
			/* NOTE: I have no idea what to do here.  Why would
			         this fail? */
		}
                pixels[color_Index] = color_Color.pixel;
		x_GcVal.foreground = color_Color.pixel;
		x_Gc[color_Index] = XCreateGC(x_Disp,x_Win,GCForeground,
		 &x_GcVal);
	}

	/* Build the tiles */
	for (tile_Index = 0;tile_Index < XTEEVEE_STATIC_TILE_COUNT;
	 tile_Index++)
	{
		if (signal_Strength == 0)
		{
			/* NOTE: Checking XQueryBestTile() returns tiles that
			         are too regular -- you can see patterns
			         emerge. */
			tile_Width = (random()%128)+64;
			tile_Height = (random()%128)+64;
		}
		else
		{
			tile_Width = x_WinAttr->width;
			tile_Height = x_WinAttr->height;
		}
		tile_Tile[tile_Index] = XCreatePixmap(x_Disp,x_Win,tile_Width,
		 tile_Height,x_WinAttr->depth);
		XCopyArea(x_Disp,hack_Pm,tile_Tile[tile_Index],x_Gc[0],0,0,
		 x_WinAttr->width,x_WinAttr->height,0,0);

		if (signal_Strength == 0)
                  {
                    if (!xim)
                      {
                        xim = XCreateImage (x_Disp, x_WinAttr->visual,
                                            x_WinAttr->depth,
                                            (x_WinAttr->depth == 1
                                             ? XYPixmap : ZPixmap),
                                            0, 0,
                                            x_WinAttr->width,
                                            x_WinAttr->height,
                                            8, 0);
                        
                        xim->data = (char *) malloc (xim->bytes_per_line *
                                                     xim->height);
                      }
                  }
                else if (xim)
                  {
                    memcpy (xim->data, orig_bits,
                            xim->bytes_per_line * xim->height);
                  }
                else
                  {
                    xim = XGetImage (x_Disp, tile_Tile[tile_Index], 0, 0,
                                     x_WinAttr->width, x_WinAttr->height, ~0L,
                                     (x_WinAttr->depth == 1
                                      ? XYPixmap : ZPixmap));
                    orig_bits = (char *) malloc (xim->bytes_per_line *
                                                 xim->height);
                    memcpy (orig_bits, xim->data,
                            xim->bytes_per_line * xim->height);
                  }

                for (tile_Y = tile_Height-1;tile_Y >= 0;tile_Y--)
                  for (tile_X = tile_Width-1;tile_X >= 0;tile_X--)
                    if (random()%100 > signal_Strength)
                      XPutPixel(xim,tile_X,tile_Y,
                                pixels[random()%XTEEVEE_STATIC_COLOR_COUNT]);
                XPutImage(x_Disp,tile_Tile[tile_Index],x_Gc[0],xim,
                          0,0,0,0,x_WinAttr->width,x_WinAttr->height);
	}

        if (xim) XDestroyImage (xim);
        if (orig_bits) free (orig_bits);

	/* Go! */
	memset(tile_Used,-1,sizeof(tile_Used));
	if (hack_Time > 0)
	{
		hack_Time += time(NULL);
	}
	while ((time(NULL) < hack_Time) || (hack_Time == 0))
	{
		/* Pick a tile */
		do
		{
			tile_Selected = random()%XTEEVEE_STATIC_TILE_COUNT;
			for (tile_Index = 0;tile_Index < sizeof(tile_Used);
			 tile_Index++)
			{
				if (tile_Used[tile_Index] == tile_Selected)
				{
					tile_Selected = -1;
					break;
				}
			}
		} while (tile_Selected == -1);
		memmove(tile_Used,tile_Used+1,sizeof(tile_Used)-1);
		tile_Used[sizeof(tile_Used)-1] = tile_Selected;

		/* Set it */
		XSetWindowBackgroundPixmap(x_Disp,x_Win,
		 tile_Tile[tile_Selected]);
		XClearWindow(x_Disp,x_Win);

		XSync(x_Disp,0);
		screenhack_handle_events(x_Disp);
		usleep(25000);
	}

	/* Free everything */
	for (color_Index = 0;color_Index < XTEEVEE_STATIC_COLOR_COUNT;
	 color_Index++)
	{
		xteevee_FreeColorForeground(x_Disp,x_WinAttr,
		 x_Gc[color_Index]);
		XFreeGC(x_Disp,x_Gc[color_Index]);
	}

	for (tile_Index = 0;tile_Index < XTEEVEE_STATIC_TILE_COUNT;
	 tile_Index++)
	{
		XFreePixmap(x_Disp, tile_Tile[tile_Index]);
	}
}

/* Vertical Roll =========================================================== */
void xteevee_Roll(Display* x_Disp,Window x_Win,XWindowAttributes* x_WinAttr,
      time_t hack_Time,Pixmap hack_Pm)
{
	GC        x_Gc;
	XGCValues x_GcVal;
	int       roll_Y = 0;
	int       roll_Speed = 0;
	int       blank_Height = x_WinAttr->height/10;

	/* Build the GC */
	x_GcVal.foreground = BlackPixel(x_Disp,0);
	x_GcVal.subwindow_mode = IncludeInferiors;
	x_Gc = XCreateGC(x_Disp,x_Win,GCForeground|GCSubwindowMode,&x_GcVal);

	/* Go! */
	if (hack_Time > 0)
	{
		hack_Time += time(NULL);
	}
	while ((roll_Y > 0) || ((time(NULL) < hack_Time) || (hack_Time == 0)))
	{
		if (roll_Y > blank_Height)
		{
			XCopyArea(x_Disp,hack_Pm,x_Win,x_Gc,
			 0,x_WinAttr->height-(roll_Y-blank_Height)-1,
			 x_WinAttr->width,roll_Y-blank_Height,
			 0,0);
		}
		XFillRectangle(x_Disp,x_Win,x_Gc,
		 0,roll_Y-blank_Height,
		 x_WinAttr->width,blank_Height);
		if (roll_Y < x_WinAttr->height)
		{
			XCopyArea(x_Disp,hack_Pm,x_Win,x_Gc,
			 0,0,
			 x_WinAttr->width,x_WinAttr->height-roll_Y,
			 0,roll_Y);
		}

		roll_Y += roll_Speed/2;
		if (roll_Speed < 50)
		{
			roll_Speed++;
		}
		if (roll_Y > x_WinAttr->height+blank_Height)
		{
			roll_Y = 0;
			roll_Speed = 0;
		}

		XSync(x_Disp,0);
		sleep(0);
		screenhack_handle_events(x_Disp);
	}

	/* Free everything */
	XFreeGC(x_Disp,x_Gc);
}

/* Color-Bars Test Pattern ================================================= */
void xteevee_Bars(Display* x_Disp,Window x_Win,XWindowAttributes* x_WinAttr,
      time_t hack_Time,Pixmap hack_Pm)
{
	GC        x_GcTop[XTEEVEE_BARS_COLOR_TOP_COUNT];
	GC        x_GcBottom[XTEEVEE_BARS_COLOR_BOTTOM_COUNT];
	XGCValues x_GcVal;
	int       color_Index;
	XColor    color_Color;
	char*     color_ColorTop[] =
	{
		"grey",
		"yellow",
		"light blue",
		"green",
		"orange",
		"red",
		"purple"
	};
	char*     color_ColorBottom[] =
	{
		"black",
		"white",
		"black",
		"black",
		"black"
	};

	/* Build the GCs */
	for (color_Index = 0;color_Index < XTEEVEE_BARS_COLOR_TOP_COUNT;
	 color_Index++)
	{
		if (XParseColor(x_Disp,x_WinAttr->colormap,
		 color_ColorTop[color_Index],&color_Color) == 0)
		{
			/* NOTE: Um, badness? */
		}
		if (XAllocColor(x_Disp,x_WinAttr->colormap,&color_Color) == 0)
		{
			/* NOTE: More badness? */
		}
		x_GcVal.foreground = color_Color.pixel;
		x_GcTop[color_Index] =
		 XCreateGC(x_Disp,x_Win,GCForeground,&x_GcVal);
	}
	for (color_Index = 0;color_Index < XTEEVEE_BARS_COLOR_BOTTOM_COUNT;
	 color_Index++)
	{
		if (XParseColor(x_Disp,x_WinAttr->colormap,
		 color_ColorBottom[color_Index],&color_Color) == 0)
		{
			/* NOTE: Um, badness? */
		}
		if (XAllocColor(x_Disp,x_WinAttr->colormap,&color_Color) == 0)
		{
			/* NOTE: More badness? */
		}
		x_GcVal.foreground = color_Color.pixel;
		x_GcBottom[color_Index] =
		 XCreateGC(x_Disp,x_Win,GCForeground,&x_GcVal);
	}

	/* Draw color-bar test pattern */
	XClearWindow(x_Disp,x_Win);
	for (color_Index = 0;color_Index < XTEEVEE_BARS_COLOR_TOP_COUNT;
	 color_Index++)
	{
		XFillRectangle(x_Disp,x_Win,x_GcTop[color_Index],
		 ((x_WinAttr->width/XTEEVEE_BARS_COLOR_TOP_COUNT)+1)*
		 color_Index,
		 0,
		 (x_WinAttr->width/XTEEVEE_BARS_COLOR_TOP_COUNT)+1,
		 (x_WinAttr->height/5)*4);
	}
	for (color_Index = 0;color_Index < XTEEVEE_BARS_COLOR_BOTTOM_COUNT;
	 color_Index++)
	{
		XFillRectangle(x_Disp,x_Win,x_GcBottom[color_Index],
		 ((x_WinAttr->width/XTEEVEE_BARS_COLOR_BOTTOM_COUNT)+1)*
		 color_Index,
		 (x_WinAttr->height/5)*4,
		 (x_WinAttr->width/XTEEVEE_BARS_COLOR_BOTTOM_COUNT)+1,
		 x_WinAttr->height-(x_WinAttr->height/5)*4);
	}

	/* Go! */
	if (hack_Time > 0)
	{
		hack_Time += time(NULL);
	}
	while ((time(NULL) < hack_Time) || (hack_Time == 0))
	{
		screenhack_handle_events(x_Disp);
		usleep(100000);
	}

	/* Free everything */
	for (color_Index = 0;color_Index < XTEEVEE_BARS_COLOR_TOP_COUNT;
	 color_Index++)
	{
		xteevee_FreeColorForeground(x_Disp,x_WinAttr,
		 x_GcTop[color_Index]);
		XFreeGC(x_Disp,x_GcTop[color_Index]);
	}
	for (color_Index = 0;color_Index < XTEEVEE_BARS_COLOR_BOTTOM_COUNT;
	 color_Index++)
	{
		xteevee_FreeColorForeground(x_Disp,x_WinAttr,
		 x_GcBottom[color_Index]);
		XFreeGC(x_Disp,x_GcBottom[color_Index]);
	}
}

/* Standard XScreenSaver entry point ======================================= */
void screenhack(Display* x_Disp,Window x_Win)
{
	XWindowAttributes x_WinAttr;
        GC                x_Gc;
        XGCValues         x_GcVal;
        Pixmap            screen_Pm;
	time_t            delay_Time;
	int               delay_Mode;
	int               delay_Between;
	int               mode_Index;
	int               mode_Count = 0;
	int               mode_Total = 0;
	char              mode_Arg[64+1];
	int               mode_Min = XTEEVEE_MODE_INCLUDE_IMPLICIT;
	struct
	{
		char* mode_Arg;
		void  (*mode_Func)(Display* x_Disp,Window x_Win,
		      XWindowAttributes* x_WinAttr,time_t hack_Time,
		      Pixmap hack_Pm);
		int   mode_Flag;
	} mode_Mode[] =
	{
		{ XTEEVEE_ARG_STATIC,xteevee_Static,XTEEVEE_MODE_EXCLUDE },
		{ XTEEVEE_ARG_ROLL,  xteevee_Roll,  XTEEVEE_MODE_EXCLUDE },
		{ XTEEVEE_ARG_BARS,  xteevee_Bars,  XTEEVEE_MODE_EXCLUDE },
		{ NULL,              NULL,          -1 }
	};

	/* Grab the screen to give us time to do whatever we want */
	XGetWindowAttributes(x_Disp,x_Win,&x_WinAttr);
	grab_screen_image(x_WinAttr.screen,x_Win);
	x_GcVal.subwindow_mode = IncludeInferiors;
	x_Gc = XCreateGC(x_Disp,x_Win,GCSubwindowMode,&x_GcVal);
	screen_Pm = XCreatePixmap(x_Disp,x_Win,x_WinAttr.width,
	 x_WinAttr.height,x_WinAttr.depth);
	XCopyArea(x_Disp,x_Win,screen_Pm,x_Gc,0,0,x_WinAttr.width,
	 x_WinAttr.height,0,0);

	/* Read the arguments */
	delay_Mode = get_integer_resource(XTEEVEE_ARG_DELAY_MODE,"Integer");
	delay_Between = get_integer_resource(XTEEVEE_ARG_DELAY_BETWEEN,
	 "Integer");
	if (!get_boolean_resource(XTEEVEE_ARG_CYCLE,"Boolean"))
	{
		delay_Mode = 0;
	}
	for (mode_Index = 0;mode_Mode[mode_Index].mode_Arg != NULL;
	 mode_Index++)
	{
		if (get_boolean_resource(mode_Mode[mode_Index].mode_Arg,
		 "Boolean") != 0)
		{
			mode_Mode[mode_Index].mode_Flag =
			 XTEEVEE_MODE_INCLUDE_IMPLICIT;
			mode_Count++;
		}
		sprintf(mode_Arg,"_%s",mode_Mode[mode_Index].mode_Arg);
		if (get_boolean_resource(mode_Arg,"Boolean") != 0)
		{
			mode_Mode[mode_Index].mode_Flag =
			 XTEEVEE_MODE_INCLUDE_EXPLICIT;
			mode_Min = XTEEVEE_MODE_INCLUDE_EXPLICIT;
			mode_Count++;
		}
		mode_Total++;
	}
	if (mode_Count == 0)
	{
		fprintf(stderr,"%s: No modes selected!\n",XTEEVEE_NAME);
		return;
	}

	/* Cycle through various modes */
	for (;;)
	{
		/* Find a mode */
		do
		{
			mode_Index = random()%mode_Total;
		} while (mode_Mode[mode_Index].mode_Flag < mode_Min);

		/* Run the hack */
		mode_Mode[mode_Index].mode_Func(x_Disp,x_Win,&x_WinAttr,
		 delay_Mode,screen_Pm);

		/* Restore the screen and wait */
		XCopyArea(x_Disp,screen_Pm,x_Win,x_Gc,0,0,x_WinAttr.width,
		 x_WinAttr.height,0,0);
		delay_Time = time(NULL)+delay_Between;
		while (time(NULL) < delay_Time)
		{
			screenhack_handle_events(x_Disp);
			usleep(100000);
		}
	}
}
