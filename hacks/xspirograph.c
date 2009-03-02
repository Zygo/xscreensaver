/* The Spiral Generator, Copyright (c) 2000 
 * by Rohit Singh <rohit_singh@hotmail.com>
 * 
 * Contains code from / To be used with:
 * xscreensaver, Copyright (c) 1992, 1995, 1996, 1997
 * Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notices appear in all copies and that both that
 * copyright notices and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <math.h>
#include "screenhack.h"
#include "erase.h"

static GC 	draw_gc;
static int 	sleep_time;
static int 	num_layers;
static unsigned int default_fg_pixel;

static void
init_tsg (Display *dpy, Window window)
{
  XGCValues	gcv;
  Colormap	cmap;
  XWindowAttributes xgwa;

  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  gcv.foreground = default_fg_pixel =
    get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  draw_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource ("background", "Background", dpy, cmap);
}


static void
go (Display *dpy, Window window,
       int radius1, int radius2,
       int d)
{
  XWindowAttributes xgwa;
  int width, height;
  int xmid, ymid;
  int x1, y1, x2, y2;
  int theta;
  int delta;

  XGetWindowAttributes (dpy, window, &xgwa);
  width  = xgwa.width;
  height = xgwa.height;
  delta = 1;
  xmid = width / 2;
  ymid = height / 2;

  x1 = xmid + radius1 - radius2 + d;
  y1 = ymid;


  for (theta = 1; theta < ( 360 * 100 ); theta++)
    {
	x2 = xmid + ((       radius1	          /* * * * *            */
                  - radius2        )		 /* This algo simulates	*/
                  * cos((      theta 		/* the rotation of a    */
                  * M_PI           ) 		/* circular disk inside */
                  / 180           )) 		/* a hollow circular 	*/
                  + (              d 		/* rim. A point on the  */
                  * cos((((  radius1 		/* disk dist d from the	*/
                  * theta          )		/* centre, traces the 	*/
                  - delta          )		/* path given by this   */
                  / radius2        )  		/* equation.	        */
                  *             M_PI		/* A deviation (error)  */
                  / 180            )            /* of delta needs to be */
                                   );           /* given, which greatly */
						/* adds to the beauty   */
	y2 = ymid + (				/* of the figure.       */
                     ( radius1 - radius2	/*			*/
                      ) * sin			/* Imperfection adds to */
                       (			/* beauty, symbolically */
                        ( theta * M_PI     	/* ...			*/
                         ) / 180		/* Algo deduced by      */
                          )			/* Rohit Singh, Jan'00  */
                           ) +                  /* based on a toy he    */
                            ( d * sin           /* used to play with    */
                             (                  /* when he was a kid.  */
                              (                 /*            * * * * */
                               (          	
                                ( radius1 * theta
                                 ) - delta
                                  ) / radius2
                                   ) * M_PI / 180
                                    )
                                     );
                          	

        XDrawLine (dpy, window, draw_gc, x1, y1, x2, y2);

	x1 = x2;
	y1 = y2;
        XFlush (dpy);
    }
}


#define min(a,b) ((a)<(b)?(a):(b))

static void
getset (Display *dpy, Window window, XColor *color, Bool *got_color)
{
  Colormap cmap;
  int width, height;
  int radius, radius1, radius2;
  double divisor;
  XWindowAttributes xgwa;
  int distance;
  int counter = 0;

  XGetWindowAttributes (dpy, window, &xgwa);
  width = xgwa.width;
  height = xgwa.height;
  cmap = xgwa.colormap;

  radius = min (width, height) / 2;

  XClearWindow (dpy, window);


  for(counter = 0; counter < num_layers; counter ++)
  {
    divisor = ((frand (3.0) + 1) * (((random() & 1) * 2) - 1));

    radius1 = radius;
    radius2 = radius / divisor + 5;
    distance = 100 + (random() % 200);

    if (mono_p)
      XSetForeground (dpy, draw_gc, default_fg_pixel);
    else
    {
      hsv_to_rgb (random () % 360, frand (1.0), frand (0.5) + 0.5,
		  &color->red, &color->green, &color->blue);
      if ((*got_color = XAllocColor (dpy, cmap, color)))
	XSetForeground (dpy, draw_gc, color->pixel);
      else
	XSetForeground (dpy, draw_gc, default_fg_pixel);
    }
    go (dpy, window, radius1, -radius2, distance);

    /* once again, with a parameter negated, just for kicks */
    if (mono_p)
      XSetForeground (dpy, draw_gc, default_fg_pixel);
    else
    {
      hsv_to_rgb (random () % 360, frand (1.0), frand (0.5) + 0.5,
		  &color->red, &color->green, &color->blue);
      if ((*got_color = XAllocColor (dpy, cmap, color)))
	XSetForeground (dpy, draw_gc, color->pixel);
      else
	XSetForeground (dpy, draw_gc, default_fg_pixel);
    }
    go (dpy, window, radius1, radius2, distance);
  }
}

static void
getset_go (Display *dpy, Window window)
{
  Bool		free_color = False;
  XColor	color;
  int		width;
  int		height;
  XWindowAttributes xgwa;
  Colormap	cmap;

  XGetWindowAttributes (dpy, window, &xgwa);

  width  = xgwa.width;
  height = xgwa.height;
  cmap   = xgwa.colormap;

  getset(dpy, window, &color, &free_color);

  XSync (dpy, False);
  screenhack_handle_events (dpy);
  sleep ( sleep_time );

  screenhack_handle_events (dpy);
  erase_full_window(dpy, window);

  if (free_color) XFreeColors (dpy, cmap, &color.pixel, 1, 0);
  XSync (dpy, False);
  screenhack_handle_events (dpy);
  sleep (1);
}

char *progclass = "XSpiroGraph";

char *defaults [] = {
  ".background: black",
  "*delay:      5",
  "*layers:     1",
  0
};

XrmOptionDescRec options [] = {   
  { "-delay",           ".delay",               XrmoptionSepArg, 0 },
  { "-layers",          ".layers",              XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};
int options_size = (sizeof (options) / sizeof (options[0]));

void
screenhack (Display *dpy, Window window)
{
  sleep_time = get_integer_resource("delay", "Integer");
  num_layers = get_integer_resource("layers", "Integer");
  init_tsg (dpy, window);
  while (1)
   getset_go (dpy, window);
}
