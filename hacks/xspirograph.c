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
 *
 * Modified (Dec 2001) by Matthew Strait <straitm@mathcs.carleton.edu>
 * Added -subdelay and -alwaysfinish
 * Prevented redrawing over existing lines
 */

#include <math.h>
#include "screenhack.h"
#include "erase.h"

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;

  GC 	draw_gc;
  int 	long_delay;
  int 	sub_sleep_time;
  int 	num_layers;
  unsigned int default_fg_pixel;
  Bool	always_finish_p;
  XColor	color;
  int got_color;

  int theta;
  float firstx, firsty;
  int x1, y1, x2, y2;

  int counter;
  int distance;
  int radius1, radius2;
  double divisor;

  enum curstate { NEW_LAYER, DRAW, ERASE1, ERASE2 } drawstate;
  eraser_state *eraser;
};


static void
init_tsg (struct state *st)
{
  XGCValues	gcv;
  Colormap	cmap;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  cmap = st->xgwa.colormap;
  gcv.foreground = st->default_fg_pixel =
    get_pixel_resource (st->dpy, cmap, "foreground", "Foreground");
  st->draw_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
  gcv.foreground = get_pixel_resource (st->dpy, cmap, "background", "Background");
}


static Bool
go (struct state *st, int radius1, int radius2, int d)
{
  int width, height;
  int xmid, ymid;
  float tmpx, tmpy;
  int delta;

  width  = st->xgwa.width;
  height = st->xgwa.height;
  delta = 1;
  xmid = width / 2;
  ymid = height / 2;

  if (st->theta == 1) {
    st->x1 = xmid + radius1 - radius2 + d;
    st->y1 = ymid;
  }

/*  for (theta = 1; / * theta < ( 360 * 100 ) * /; theta++) */
                  /* see below about alwaysfinish */
    {
	tmpx = xmid + ((       radius1	          /* * * * *            */
                  - radius2        )		 /* This algo simulates	*/
                  * cos((      st->theta 		/* the rotation of a    */
                  * M_PI           ) 		/* circular disk inside */
                  / 180           )) 		/* a hollow circular 	*/
                  + (              d 		/* rim. A point on the  */
                  * cos((((  radius1 		/* disk dist d from the	*/
                  * st->theta      )		/* centre, traces the 	*/
                  - delta          )		/* path given by this   */
                  / radius2        )  		/* equation.	        */
                  *             M_PI		/* A deviation (error)  */
                  / 180            )            /* of delta needs to be */
                                   );           /* given, which greatly */
						/* adds to the beauty   */
	tmpy = ymid + (				/* of the figure.       */
                     ( radius1 - radius2	/*			*/
                      ) * sin			/* Imperfection adds to */
                       (			/* beauty, symbolically */
                        ( st->theta * M_PI     	/* ...			*/
                         ) / 180		/* Algo deduced by      */
                          )			/* Rohit Singh, Jan'00  */
                           ) +                  /* based on a toy he    */
                            ( d * sin           /* used to play with    */
                             (                  /* when he was a kid.  */
                              (                 /*            * * * * */
                               (          	
                                ( radius1 * st->theta
                                 ) - delta
                                  ) / radius2
                                   ) * M_PI / 180
                                    )
                                     );
        
	/*makes integers from the calculated values to do the drawing*/
	st->x2 = tmpx;
	st->y2 = tmpy;

	/*stores the first values for later reference*/
	if(st->theta == 1)
	{
  		st->firstx = tmpx;
  		st->firsty = tmpy;
	}

        if (st->theta != 1)
          XDrawLine (st->dpy, st->window, st->draw_gc, 
                     st->x1, st->y1, st->x2, st->y2);

	st->x1 = st->x2;
	st->y1 = st->y2;

	/* compares the exact values calculated to the first
	   exact values calculated */
	/* this will break when nothing new is being drawn */
	if(tmpx == st->firstx && tmpy == st->firsty && st->theta != 1) {
          st->firstx = st->firsty = 0;
          st->theta = 1;
          return True;
        }

	/* this will break after 36000 iterations if 
	   the -alwaysfinish option is not specified */
	if(!st->always_finish_p && st->theta > ( 360 * 100 ) ) {
          st->firstx = st->firsty = 0;
          st->theta = 1;
          return True;
        }
    }

    st->theta++;

    return False;
}


#define min(a,b) ((a)<(b)?(a):(b))


static void
pick_new (struct state *st)
{
  int radius = min (st->xgwa.width, st->xgwa.height) / 2;
  st->divisor = ((frand (3.0) + 1) * (((random() & 1) * 2) - 1));
  st->radius1 = radius;
  st->radius2 = radius / st->divisor + 5;
  st->distance = 100 + (random() % 200);
  st->theta = 1;
}


static void *
xspirograph_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;
  st->long_delay = get_integer_resource(st->dpy, "delay", "Integer");
  st->sub_sleep_time = get_integer_resource(st->dpy, "subdelay", "Integer");
  st->num_layers = get_integer_resource(st->dpy, "layers", "Integer");
  st->always_finish_p = get_boolean_resource (st->dpy, "alwaysfinish", "Boolean");
  
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  init_tsg (st);
  st->theta = 1;
  st->drawstate = NEW_LAYER;

  return st;
}


static void
new_colors (struct state *st)
{
  if (mono_p)
    XSetForeground (st->dpy, st->draw_gc, st->default_fg_pixel);
  else
    {
      hsv_to_rgb (random () % 360, frand (1.0), frand (0.5) + 0.5,
		  &st->color.red, &st->color.green, &st->color.blue);
      if ((st->got_color = XAllocColor (st->dpy, st->xgwa.colormap,
                                        &st->color)))
	XSetForeground (st->dpy, st->draw_gc, st->color.pixel);
      else
	XSetForeground (st->dpy, st->draw_gc, st->default_fg_pixel);
    }
}



static unsigned long
xspirograph_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  Bool free_color = False;
  Bool flip_p = (st->counter & 1);
  int i;

  switch (st->drawstate) {
    case ERASE1:
      /* 5 sec delay before starting the erase */
      st->drawstate = ERASE2;
      /* shouldn't this use the configured long_delay value??? */
      return (st->long_delay == 0 ? 0 : 5000000);

    case ERASE2:
      /* erase, delaying 1/50th sec between frames */
      st->eraser = erase_window(st->dpy, st->window, st->eraser);
      if (st->eraser)
	/* shouldn't this be a configured pause??? */
	return 20000;
      st->drawstate = NEW_LAYER;
      /* just finished erasing -- leave screen black for 1 sec */
      return (st->long_delay == 0 ? 0 : 1000000);

    case DRAW:
      /* most common case put in front */
      for (i = 0; i < 1000; i++) {
        if (go(st, st->radius1, (flip_p ? st->radius2 : -st->radius2),
	       st->distance)) {
	  st->drawstate = NEW_LAYER;
	  break;
	}
      }
      /* Next draw is delayed sleep_time (initialized value)*/
      return st->sub_sleep_time;

    case NEW_LAYER:
      /* Increment counter */
      st->counter++;
      if (st->counter > (2 * st->num_layers)) {
	/* reset to zero, free, and erase next time through */
	st->counter = 0;
	if (free_color)
	  XFreeColors (st->dpy, st->xgwa.colormap, &st->color.pixel, 1, 0);
	st->drawstate = ERASE1;
      } else {
	/* first, third, fifth, ... time through */
	if (!flip_p)
	  pick_new (st);

	new_colors (st);
	st->drawstate = DRAW;
      }
      /* No delay to the next draw */
      return 0;

    default:
      /* OOPS!! */
      fprintf(stderr, "%s: invalid state\n", progname);
      exit(1);
  }

  return st->sub_sleep_time;
  /* notreached */
}


static void
xspirograph_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
}

static Bool
xspirograph_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
xspirograph_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *xspirograph_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*fpsSolid:		true",
  "*delay:      	5",
  "*subdelay:   	20000",
  "*layers:     	2",
  "*alwaysfinish:	false",
#ifdef USE_IPHONE
  "*ignoreRotation:     True",
#endif
  0
};

static XrmOptionDescRec xspirograph_options [] = {   
  { "-delay",           ".delay",               XrmoptionSepArg, 0 },
  { "-subdelay",        ".subdelay",            XrmoptionSepArg, 0 },
  { "-layers",          ".layers",        	XrmoptionSepArg, 0 },
  { "-alwaysfinish",    ".alwaysfinish",	XrmoptionNoArg, "true"},
  { "-noalwaysfinish",  ".alwaysfinish",	XrmoptionNoArg, "false"},
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("XSpirograph", xspirograph)
