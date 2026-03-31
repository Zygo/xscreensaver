/* truchet --- curved and straight tilings
 * Copyright (c) 1998 Adrian Likins <adrian@gimp.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This screensaver draws two varieties of truchet patterns, a curved one and
   a straight one. There are lots and lots of command line options to play
   with.

   If your running remotely or on a slow machine or slow xserver, some of the
   settings will be way too much. The default settings should be okay though. 

   This screensaver doesnt use anything bizarre or special at all, just a few
   standard xlib calls.

   A few suggested commandline combos..All these were tested on a k6-200
   running XFree86 3.3 on a ark2000, so your mileage may vary...

      truchet -delay 200 -no-curves
      truchet -delay 500 -no-curves -square -no-erase
      truchet -delay 500 -no-erase -square -erase-count 5
      truchet -scroll
      truchet -scroll -no-erase -anim-step-size 9
      truchet -delay 200 -no-angles -min-width 36 -max-width 36
      truchet -delay 200 -no-curves -min-width 12 -max-width 12
      truchet -delay 200 -no-curves -min-width 36 -max-width 36 -no-erase
      truchet -delay 100 -min-width 256 -max-width 512 -no-erase \
              -min-linewidth 96 -root
      truchet -min-width 64 -max-width 128 -no-erase -max-linewidth 4 \
              -root -no-angles
      truchet -min-width 64 -max-width 128 -no-erase -max-linewidth 4 \
              -root -no-curves -delay 25
 */

#include "screenhack.h"

#define MAXRATIO 2

static const char *truchet_defaults [] = {
  "*minWidth:                 40",
  "*minHeight:                40",
  "*max-Width:                150",
  "*max-Height:               150",
  "*maxLineWidth:             25",
  "*minLineWidth:             2",
  "*erase:                    True",
  "*eraseCount:               25",        
  "*square:                   True",
  "*delay:                    400000",
  "*curves:                   True",
  "*angles:                   True",
  "*scroll:                   False",
  "*scroll-overlap:           400",
  "*anim-delay:               100",
  "*anim-step-size:           3",
  "*randomize:		      true",
#ifdef HAVE_MOBILE
  "*ignoreRotation:           True",
#endif
   0
};

/* options passed to this program */
static XrmOptionDescRec truchet_options [] = {
  { "-min-width",      ".minWidth",       XrmoptionSepArg, 0 },
  { "-max-height",     ".max-Height",      XrmoptionSepArg, 0 },
  { "-max-width",      ".max-Width",       XrmoptionSepArg, 0 },
  { "-min-height",     ".minHeight",      XrmoptionSepArg, 0 },
  { "-max-linewidth",  ".maxLineWidth",   XrmoptionSepArg, 0 },
  { "-min-linewidth",  ".minLineWidth",   XrmoptionSepArg, 0 },
  { "-erase",          ".erase",          XrmoptionNoArg, "True" },
  { "-no-erase",       ".erase",          XrmoptionNoArg, "False" },
  { "-erase-count",    ".eraseCount",     XrmoptionSepArg, 0 },
  { "-square",         ".square",         XrmoptionNoArg, "True" },
  { "-not-square",     ".square",         XrmoptionNoArg, "False" },
  { "-curves",         ".curves",         XrmoptionNoArg, "True" },
  { "-angles",         ".angles",         XrmoptionNoArg,  "True" },
  { "-no-angles",      ".angles",         XrmoptionNoArg,  "False" },
  { "-no-curves",      ".curves",         XrmoptionNoArg, "False" },
  { "-delay",          ".delay",          XrmoptionSepArg, 0 },
  { "-scroll",         ".scroll",         XrmoptionNoArg, "True" },
  { "-scroll-overlap", ".scroll-overlap", XrmoptionSepArg, 0 },
  { "-anim-delay",     ".anim-delay",     XrmoptionSepArg, 0 },
  { "-anim-step-size",  ".anim-step-size", XrmoptionSepArg, 0 },
  { "-randomize",	".randomize",	  XrmoptionNoArg, "True" },
  { 0, 0, 0, 0 }
};

struct state {
  Display *dpy;
  Window window;

  XGCValues gcv;
  GC agc, bgc;
  int linewidth;
  int width, height;
  XWindowAttributes xgwa;
  Pixmap frame;
  int overlap;

  int maxlinewidth;
  int minlinewidth;
  int minwidth;
  int minheight;
  int max_height; 
  int max_width; 
  int delay;
  int count;
  int anim_delay;
  int anim_step_size;

  Colormap cmap;
  XColor fgc;
  Bool curves;
  Bool square;
  Bool angles;
  Bool erase;
  Bool eraseCount;
  Bool scroll;
  int scrolling;
};

static void draw_truchet(struct state *st);
static void draw_angles(struct state *st);
static void scroll_area(struct state *st, int step_size);

static void draw_angles(struct state *st)
{
  int cx = 0, cy = 0;
  
  while((st->xgwa.height+st->overlap) > cy*st->height)
	{
	  while((st->xgwa.width+st->overlap) > cx*st->width)
	    {
	      if(random()%2)
	      {
		/* block1 */
		XDrawLine(st->dpy,st->frame,st->agc,
			  (cx*st->width)+(st->width/2),
			  (cy*st->height), 
			  (cx*st->width)+(st->width),
			  (cy*st->height)+(st->height/2));
		XDrawLine(st->dpy,st->frame,st->agc,
			  (cx*st->width), 
			  (cy*st->height)+(st->height/2),
			  (cx*st->width)+(st->width/2),
			  (cy*st->height)+(st->height));
	      }
	    else
	      {
		/* block 2 */
		XDrawLine(st->dpy,st->frame,st->agc, 
			  (cx*st->width)+(st->width/2),
			  (cy*st->height),
			  (cx*st->width),
			  (cy*st->height)+(st->height/2));
		XDrawLine(st->dpy,st->frame,st->agc,
			  (cx*st->width)+(st->width),
			  (cy*st->height)+(st->height/2),
			  (cx*st->width)+(st->width/2),
			  (cy*st->height)+(st->height)); 
	      }
	      cx++;
	    }
	  cy++;
	  cx=0;
	}
}
  

static void draw_truchet(struct state *st)
{
  int cx = 0, cy = 0;

  while(st->xgwa.height+st->overlap > cy*st->height)
	{
	  while(st->xgwa.width+st->overlap > cx*st->width)
	    {
	      if(random()%2)
	      {
		/* block1 */
		XDrawArc(st->dpy, st->frame, st->agc,
			 ((cx*st->width)-(st->width/2)),
			 ((cy*st->height)-(st->height/2)),
			 st->width,
			 st->height,
			 0, -5760);
		XDrawArc(st->dpy,st->frame, st->agc,
			 ((cx*st->width)+(st->width/2)),
			 ((cy*st->height)+(st->height/2)),
			 st->width,
			 st->height,
			 11520,
			 -5760);
	      }
	    else
	      {
		/* block 2 */
		XDrawArc(st->dpy,st->frame,st->agc,
			 ((cx*st->width)+(st->width/2)),
			 ((cy*st->height)-(st->height/2)),
			 st->width,
			 st->height,
			 17280,
			 -5760);
		XDrawArc(st->dpy,st->frame,st->agc,
			 ((cx*st->width)-(st->width/2)),
			 ((cy*st->height)+(st->height/2)),
			 st->width,
			 st->height,
			 0,
			 5760);
	      }
	      cx++;
	    }
	  cy++;
	  cx=0;
	}
}


static void scroll_area(struct state *st, int step_size)
{
  int scrollcount_x;
  int scrollcount_y;
  int offset;
  int scroll;
  int direction;
  int progress;

  offset=st->overlap/2;
  scroll=st->overlap/4;
  
  /* This runs in a loop, starting with
   *  st->scrolling = (scroll / st->anim_step_size) * 4 - 1;
   * and going all the way down to st->scrolling = 0.
   */

  /* if anyone knows a good way to generate
   * a more random scrolling motion... */

  direction = st->scrolling / (scroll / st->anim_step_size);
  progress = (st->scrolling % (scroll / st->anim_step_size)) * st->anim_step_size;

  if (direction & 1) {
    scrollcount_x = progress - scroll;
    scrollcount_y = progress;
  } else {
    scrollcount_x = -progress;
    scrollcount_y = progress - scroll;
  }

  if (direction & 2) {
    scrollcount_x = -scrollcount_x;
    scrollcount_y = -scrollcount_y;
  }

  XCopyArea(st->dpy, st->frame, st->window, st->agc,scrollcount_x+offset,scrollcount_y+offset, st->xgwa.width, st->xgwa.height, 0,0);
}


static void *
truchet_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));

  st->dpy = dpy;
  st->window = window;

  st->maxlinewidth = get_integer_resource (st->dpy, "maxLineWidth", "Integer");
  st->minlinewidth = get_integer_resource (st->dpy, "minLineWidth", "Integer");
  st->minwidth = get_integer_resource (st->dpy, "minWidth", "Integer");
  st->minheight = get_integer_resource (st->dpy, "minHeight", "Integer");
  st->max_width = get_integer_resource (st->dpy, "max-Width", "Integer"); 
  st->max_height = get_integer_resource (st->dpy, "max-Height", "Integer" ); 
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->eraseCount = get_integer_resource (st->dpy, "eraseCount", "Integer");
  st->square = get_boolean_resource (st->dpy, "square", "Boolean");
  st->curves = get_boolean_resource (st->dpy, "curves", "Boolean");
  st->angles = get_boolean_resource (st->dpy, "angles", "Boolean");
  st->erase = get_boolean_resource (st->dpy, "erase", "Boolean");
  st->scroll = get_boolean_resource (st->dpy, "scroll", "Boolean");
  st->overlap = get_integer_resource (st->dpy, "scroll-overlap", "Integer");
  st->anim_delay = get_integer_resource (st->dpy, "anim-delay", "Integer");
  st->anim_step_size = get_integer_resource (st->dpy, "anim-step-size", "Integer");

  if (get_boolean_resource(st->dpy, "randomize", "Randomize"))
    {
      int i = (random() % 12);
      switch(i) {
      case 0:
	break;
      case 1:
	st->curves = False;
	break;
      case 2:
	st->curves = False;
	st->square = True;
	st->erase = False;
	break;
      case 3:
	st->square = True;
	st->erase = False;
	st->eraseCount = 5;
	break;
      case 4:
	st->scroll = True;
	break;
      case 5:
	st->scroll = True;
	st->erase = False;
	st->anim_step_size = 9;
	break;
      case 6:
	st->angles = False;
	st->minwidth = st->max_width = 36;
	break;
      case 7:
	st->curves = False;
	st->minwidth = st->max_width = 12;
	break;
      case 8:
	st->curves = False;
	st->erase = False;
	st->minwidth = st->max_width = 36;
	break;
      case 9:
	st->erase = False;
	st->minwidth = 256;
	st->max_width = 512;
	st->minlinewidth = 96;
	break;
      case 10:
	st->angles = False;
	st->minwidth = 64;
	st->max_width = 128;
	st->maxlinewidth = 4;
	break;
      case 11:
	st->curves = False;
	st->minwidth = 64;
	st->max_width = 128;
	st->maxlinewidth = 4;
	break;
      default:
	abort();
	break;
      }
    }

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  st->gcv.foreground = BlackPixel(st->dpy,0);
  st->gcv.background = WhitePixel(st->dpy,0);
  st->gcv.line_width = 25;
  st->cmap = st->xgwa.colormap;

  st->gcv.foreground = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                      "background", "Background");

  st->bgc = XCreateGC (st->dpy, st->window, GCForeground, &st->gcv);
  st->agc = XCreateGC(st->dpy, st->window, GCForeground, &st->gcv);

  XFillRectangle(st->dpy, st->window, st->bgc, 0, 0, st->xgwa.width, st->xgwa.height);

 
  st->width=60;
  st->height=60;
  st->linewidth=1;
  st->count=0;
  XSetForeground(st->dpy, st->agc, st->gcv.background);
  
  
  st->frame = XCreatePixmap(st->dpy,st->window, st->xgwa.width+st->overlap, st->xgwa.height+st->overlap, st->xgwa.depth); 
  XFillRectangle(st->dpy, st->frame, st->bgc, 0, 0, 
                 st->xgwa.width + st->overlap, 
                 st->xgwa.height + st->overlap);
  
  return st;
}

static unsigned long
truchet_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->scrolling)
    {
      st->scrolling--;
      scroll_area(st, st->anim_step_size);
      return st->anim_delay*1000;
    }

  if (!mono_p)
    {
      /* XXX there are probably bugs with this. */
      /* could be...I just borrowed this code from munch */

      st->fgc.red = random() % 65535;
      st->fgc.green = random() % 65535;
      st->fgc.blue = random() % 65535;
	
      if (XAllocColor(st->dpy, st->cmap, &st->fgc)) 
        {
          XSetForeground(st->dpy, st->agc, st->fgc.pixel);
        }
      else
        {
          /* use white if all else fails  */
          XSetForeground(st->dpy,st->agc, st->gcv.background);
        }
    }

      
      

  /* generate a random line width */
  st->linewidth=(random()% st->maxlinewidth);

  /* check for lower bound */
  if(st->linewidth < st->minlinewidth)
    st->linewidth = st->minlinewidth;

  /* try to get an odd linewidth as it seem to work a little better */
  if(st->linewidth%2)
    st->linewidth++;

  /* grab a random height and width */ 
  st->width=(random()%st->max_width);
  st->height=(random()%st->max_height);

  /* make sure we dont get a 0 height or width */
  if(st->width == 0 || st->height == 0)
    {
      st->height=st->max_height;
      st->width=st->max_width;
    }


  /* check for min height and width */
  if(st->height < st->minheight)
    {
      st->height=st->minheight;
    }
  if(st->width < st->minwidth)
    {
      st->width=st->minwidth;
    }

  /* if tiles need to be square, fix it... */
  if(st->square)
    st->height=st->width;

  /* check for sane aspect ratios */
  if((st->width/st->height) > MAXRATIO) 
    st->height=st->width;
  if((st->height/st->width) > MAXRATIO)
    st->width=st->height;
      
  /* to avoid linewidths of zero */
  if(st->linewidth == 0 || st->linewidth < st->minlinewidth)
    st->linewidth = st->minlinewidth;

  /* try to keep from getting line widths that would be too big */
  if(st->linewidth > 0 && st->linewidth >= (st->height/5))
    st->linewidth = st->height/5;
  
  XSetLineAttributes(st->dpy, st->agc, st->linewidth, LineSolid, CapRound, JoinRound);

  if(st->erase || (st->count >= st->eraseCount))
    {
      /*  XClearWindow(dpy,window); */
      XFillRectangle(st->dpy, st->frame, st->bgc, 0, 0, st->xgwa.width+st->overlap, st->xgwa.height+st->overlap);
      st->count=0;
    }
            
  if(!st->scroll)
    st->overlap=0;
            
  /* do the fun stuff...*/
  if(st->curves && st->angles)
    {
      if(random()%2)
        draw_truchet(st);
      else
        draw_angles(st);
    }
  else if(st->curves && !st->angles)
    draw_truchet(st);
  else if(!st->curves && st->angles)
    draw_angles(st);

   
  st->count++;

  if(st->scroll)
    {
      st->scrolling = ((st->overlap / 4) / st->anim_step_size) * 4;
      return 0;
    }

  XCopyArea(st->dpy,st->frame,st->window,st->agc,0,0,st->xgwa.width,st->xgwa.height,0,0);

  return st->delay;
}

static void
truchet_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->width = w;
  st->height = h;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
}

static Bool
truchet_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
truchet_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (dpy, st->agc);
  XFreeGC (dpy, st->bgc);
  free (st);
}


XSCREENSAVER_MODULE ("Truchet", truchet)

