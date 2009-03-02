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


char *progclass="Truchet";

char *defaults [] = {
  "*minWidth:                 40",
  "*minHeight:                40",
  "*max-Width:                150",
  "*max-Height:               150",
  "*maxLineWidth:             25",
  "*minLineWidth:             2",
  "*erase:                    True",
  "*eraseCount:               25",        
  "*square:                   True",
  "*delay:                    1000",
  "*curves:                   True",
  "*angles:                   True",
  "*angles-and-curves:        True",
  "*scroll:                   False",
  "*scroll-overlap:           400",
  "*anim-delay:               100",
  "*anim-step-size:           3",
  "*randomize:		      false",
   0
};

/* options passed to this program */
XrmOptionDescRec options [] = {
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

static GC agc, bgc;
static int linewidth;
static int width, height;
static XWindowAttributes xgwa;
static Pixmap frame;
static int overlap;

static void draw_truchet(Display *disp, Window win);
static void draw_angles(Display *disp, Window win);
static void scroll_area(Display *disp, Window win, int delay, int step_size);

static void draw_angles(Display *disp, Window win)
{
  int countX;
  int countY;

  countX=0;
  countY=0;
  
  while((xgwa.height+overlap) > countY*height)
	{
	  while((xgwa.width+overlap) > countX*width)
	    {
	      if(random()%2)
	      {
		/* block1 */
		XDrawLine(disp,frame,agc,
			  (countX*width)+(width/2),
			  (countY*height), 
			  (countX*width)+(width),
			  (countY*height)+(height/2));
		XDrawLine(disp,frame,agc,
			  (countX*width), 
			  (countY*height)+(height/2),
			  (countX*width)+(width/2),
			  (countY*height)+(height));
	      }
	    else
	      {
		/* block 2 */
		XDrawLine(disp,frame,agc, 
			  (countX*width)+(width/2),
			  (countY*height),
			  (countX*width),
			  (countY*height)+(height/2));
		XDrawLine(disp,frame,agc,
			  (countX*width)+(width),
			  (countY*height)+(height/2),
			  (countX*width)+(width/2),
			  (countY*height)+(height)); 
	      }
	      countX++;
	    }
	  countY++;
	  countX=0;
	}

  countX=0;
  countY=0;
}
  

static void draw_truchet(Display *disp, Window win)
{
  int countX;
  int countY;


  countX=0;
  countY=0;


  while(xgwa.height+overlap > countY*height)
	{
	  while(xgwa.width+overlap > countX*width)
	    {
	      if(random()%2)
	      {
		/* block1 */
		XDrawArc(disp, frame, agc,
			 ((countX*width)-(width/2)),
			 ((countY*height)-(height/2)),
			 width,
			 height,
			 0, -5760);
		XDrawArc(disp,frame, agc,
			 ((countX*width)+(width/2)),
			 ((countY*height)+(height/2)),
			 width,
			 height,
			 11520,
			 -5760);
	      }
	    else
	      {
		/* block 2 */
		XDrawArc(disp,frame,agc,
			 ((countX*width)+(width/2)),
			 ((countY*height)-(height/2)),
			 width,
			 height,
			 17280,
			 -5760);
		XDrawArc(disp,frame,agc,
			 ((countX*width)-(width/2)),
			 ((countY*height)+(height/2)),
			 width,
			 height,
			 0,
			 5760);
	      }
	      countX++;
	    }
	  countY++;
	  countX=0;
	}
   countX=0;
   countY=0;
}
/* this is the function called for your screensaver */
void screenhack(Display *disp, Window win)
{
  XGCValues gcv;
  int countX;
  int countY;
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
  

  maxlinewidth = get_integer_resource ("maxLineWidth", "Integer");
  minlinewidth = get_integer_resource ("minLineWidth", "Integer");
  minwidth = get_integer_resource ("minWidth", "Integer");
  minheight = get_integer_resource ("minHeight", "Integer");
  max_width = get_integer_resource ("max-Width", "Integer"); 
  max_height = get_integer_resource ("max-Height", "Integer" ); 
  delay = get_integer_resource ("delay", "Integer");
  eraseCount = get_integer_resource ("eraseCount", "Integer");
  square = get_boolean_resource ("square", "Boolean");
  curves = get_boolean_resource ("curves", "Boolean");
  angles = get_boolean_resource ("angles", "Boolean");
  erase = get_boolean_resource ("erase", "Boolean");
  scroll = get_boolean_resource ("scroll", "Boolean");
  overlap = get_integer_resource ("scroll-overlap", "Integer");
  anim_delay = get_integer_resource ("anim-delay", "Integer");
  anim_step_size = get_integer_resource ("anim-step-size", "Integer");

  if (get_boolean_resource("randomize", "Randomize"))
    {
      int i = (random() % 12);
      switch(i) {
      case 0:
	break;
      case 1:
	curves = False;
	break;
      case 2:
	curves = False;
	square = True;
	erase = False;
	break;
      case 3:
	square = True;
	erase = False;
	eraseCount = 5;
	break;
      case 4:
	scroll = True;
	break;
      case 5:
	scroll = True;
	erase = False;
	anim_step_size = 9;
	break;
      case 6:
	angles = False;
	minwidth = max_width = 36;
	break;
      case 7:
	curves = False;
	minwidth = max_width = 12;
	break;
      case 8:
	curves = False;
	erase = False;
	minwidth = max_width = 36;
	break;
      case 9:
	erase = False;
	minwidth = 256;
	max_width = 512;
	minlinewidth = 96;
	break;
      case 10:
	angles = False;
	minwidth = 64;
	max_width = 128;
	maxlinewidth = 4;
	break;
      case 11:
	curves = False;
	minwidth = 64;
	max_width = 128;
	maxlinewidth = 4;
	break;
      default:
	abort();
	break;
      }
    }

  XGetWindowAttributes (disp, win, &xgwa);
  gcv.foreground = BlackPixel(disp,0);
  gcv.background = WhitePixel(disp,0);
  gcv.line_width = 25;
  cmap = xgwa.colormap;

  gcv.foreground = get_pixel_resource("background", "Background",
				      disp, xgwa.colormap);

  bgc = XCreateGC (disp, win, GCForeground, &gcv);
  agc = XCreateGC(disp, win, GCForeground, &gcv);

  XFillRectangle(disp, win, bgc, 0, 0, xgwa.width, xgwa.height);

 
  width=60;
  height=60;
  linewidth=1;
  countX=0;
  countY=0;
  count=0;
  XSetForeground(disp, agc, gcv.background);
  
  
  frame = XCreatePixmap(disp,win, xgwa.width+overlap, xgwa.height+overlap, xgwa.depth); 
  

  while(1)
    {
      if (!mono_p)
	{
	/* XXX there are probably bugs with this. */
        /* could be...I just borrowed this code from munch */

	fgc.red = random() % 65535;
	fgc.green = random() % 65535;
	fgc.blue = random() % 65535;
	
	if (XAllocColor(disp, cmap, &fgc)) 
	  {
	    XSetForeground(disp, agc, fgc.pixel);
	  }
	else
	  {
	    /* use white if all else fails  */
	    XSetForeground(disp,agc, gcv.background);
	  }
      }

      
      

      /* generate a random line width */
      linewidth=(random()% maxlinewidth);

      /* check for lower bound */
      if(linewidth < minlinewidth)
	linewidth = minlinewidth;

      /* try to get an odd linewidth as it seem to work a little better */
      if(linewidth%2)
	linewidth++;

      /* grab a random height and width */ 
      width=(random()%max_width);
      height=(random()%max_height);

      /* make sure we dont get a 0 height or width */
      if(width == 0 || height == 0)
	{
	  height=max_height;
	  width=max_width;
	}


      /* check for min height and width */
      if(height < minheight)
	{
	  height=minheight;
	}
      if(width < minwidth)
	{
	  width=minwidth;
	}

      /* if tiles need to be square, fix it... */
      if(square)
	height=width;

      /* check for sane aspect ratios */
      if((width/height) > MAXRATIO) 
	height=width;
      if((height/width) > MAXRATIO)
	width=height;
      
      /* to avoid linewidths of zero */
      if(linewidth == 0 || linewidth < minlinewidth)
	linewidth = minlinewidth;

      /* try to keep from getting line widths that would be too big */
      if(linewidth > 0 && linewidth >= (height/5))
	linewidth = height/5;
  
      XSetLineAttributes(disp, agc, linewidth, LineSolid, CapRound, JoinRound);

      if(erase || (count >= eraseCount))
	{
	  /*  XClearWindow(disp,win); */
	  XFillRectangle(disp, frame, bgc, 0, 0, xgwa.width+overlap, xgwa.height+overlap);
	  count=0;
	}
            
      if(!scroll)
	overlap=0;
            
      /* do the fun stuff...*/
      if(curves && angles)
	{
	  if(random()%2)
	    draw_truchet(disp,win);
	  else
	    draw_angles(disp,win);
	}
      else if(curves && !angles)
	draw_truchet(disp,win);
      else if(!curves && angles)
	draw_angles(disp,win);

   
      XCopyArea(disp,frame,win,agc,0,0,xgwa.width,xgwa.height,0,0);
      if(scroll)
	{
	  scroll_area(disp,win,anim_delay,anim_step_size);
	  delay = 0;
	}
      else
	XSync(disp,True);
      
      /* the delay to try to minimize seizures */
      usleep((delay*1000)); 
      count++;
      
    }

}

static void scroll_area(Display *disp, Window win, int delay, int step_size)
{

  int scrollcount_x;
  int scrollcount_y;
  int offset;
  int scroll;
  /* note local delay overirdes static delay cause... */


  scrollcount_x=0;
  scrollcount_y=0;

  offset=overlap/2;
  scroll=overlap/4;
  
      /* if anyone knows a good way to generate a more random scrolling motion... */
  while(scrollcount_x <= scroll)
    {
      XCopyArea(disp, frame, win, agc,scrollcount_x+offset,scrollcount_y+offset, xgwa.width, xgwa.height, 0,0);
      XSync(disp,True);
      scrollcount_x=scrollcount_x+step_size;
      scrollcount_y=scrollcount_y+step_size;
      usleep(1000*delay); 
    }
  while(scrollcount_x >= 0)
    {
      XCopyArea(disp, frame, win, agc,scrollcount_x+offset,scrollcount_y+offset, xgwa.width, xgwa.height, 0,0);
      XSync(disp,True);
      scrollcount_y=scrollcount_y+step_size;
      scrollcount_x=scrollcount_x-step_size;
      usleep(1000*delay); 
    }
  while(scrollcount_y >= scroll)
    {
      XCopyArea(disp, frame, win, agc,scrollcount_x+offset,scrollcount_y+offset, xgwa.width, xgwa.height, 0,0);
      XSync(disp,True);
      scrollcount_x=scrollcount_x-step_size;
      scrollcount_y=scrollcount_y-step_size;
      usleep(1000*delay); 
    }
  while(scrollcount_y > 0)
    {
      XCopyArea(disp, frame, win, agc,scrollcount_x+offset,scrollcount_y+offset, xgwa.width, xgwa.height, 0,0);
      XSync(disp,True);
      scrollcount_y=scrollcount_y-step_size;
      scrollcount_x=scrollcount_x+step_size;
      usleep(1000*delay); 
    }
  
  XSync(disp,True);
  scrollcount_x=0;
  scrollcount_y=0;
  
}
