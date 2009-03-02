/* erase.c: Erase the screen in various more or less interesting ways.
 * (c) 1997 by Johannes Keukelaar <johannes@nada.kth.se>
 * Permission to use in any way granted. Provided "as is" without expressed
 * or implied warranty. NO WARRANTY, NO EXPRESSION OF SUITABILITY FOR ANY
 * PURPOSE. (I.e.: Use in any way, but at your own risk!)
 */

#include "utils.h"
#include "yarandom.h"
#include "usleep.h"
#include "resources.h"

#define NUM_MODES 8

void
erase_window(Display *dpy, Window window, GC gc,
	     int width, int height, int mode, int delay)
{
  int *clear_lines;
  int i, j, line, num_lines=0, granularity, max_num;

  max_num = 2*height;
  if(2*width>max_num)
    max_num = 2*width;

  clear_lines = (int *)calloc(max_num, sizeof(int));
  if(clear_lines)
    {
      if(mode<0 || mode>=NUM_MODES)
	mode = random()%NUM_MODES;
      granularity = 25;
      switch(mode)
	{
	case 0:				/* clear random horizontal lines */
	  for(i = 0; i < height; i++)
	    clear_lines[i] = i;
	  for(i = 0; i < height; i++)
	    {
	      int t, r;
	      t = clear_lines[i];
	      r = random()%height;
	      clear_lines[i] = clear_lines[r];
	      clear_lines[r] = t;
	    }
	  num_lines = height;
	  break;

	case 1:				/* clear random vertical lines */
	  for(i = 0; i < width; i++)
	    clear_lines[i] = i+height;
	  for(i = 0; i < width; i++)
	    {
	      int t, r;
	      t = clear_lines[i];
	      r = random()%width;
	      clear_lines[i] = clear_lines[r];
	      clear_lines[r] = t;
	    }
	  num_lines = width;
	  break;

	case 2:					/* 4 sequential wipes,
						   L-R, T-B, R-L, B-T. */
	  for(i = 0; i < width/2; i++)
	    clear_lines[i] = i*2+height;
	  for(i = 0; i < height/2; i++)
	    clear_lines[i+width/2] = i*2;
	  for(i = 0; i < width/2; i++)
	    clear_lines[i+width/2+height/2] = width-i*2-(width%2?0:1)+height;
	  num_lines = width+height/2;
	  granularity = 4;
	  break;

	case 3:					/* 4 parallel wipes,
						   L-R, T-B, R-L, B-T. */
	  for(i = 0; i < max_num/4; i++)
	    {
	      clear_lines[i*4] = i*2;
	      clear_lines[i*4+1] = height-i*2-(height%2?0:1);
	      clear_lines[i*4+2] = height+i*2;
	      clear_lines[i*4+3] = height+width-i*2-(width%2?0:1);
	    }
	  num_lines = max_num;
	  granularity = 4;
	  break;

	case 4:					/* flutter wipe L-R */
	  j = 0;
	  for(i = 0; i < width*2; i++)
	    {
	      line = (i/16)*16-(i%16)*15;
	      if(line>=0 && line<width)
		{
		  clear_lines[j] = height+line;
		  j++;
		}
	    }
	  num_lines = width;
	  granularity = 4;
	  break;

	case 5:					/* flutter wipe R-L */
	  j = 0;
	  for(i = width*2; i >= 0; i--)
	    {
	      line = (i/16)*16-(i%16)*15;
	      if(line>=0 && line<width)
		{
		  clear_lines[j] = height+line;
		  j++;
		}
	    }
	  num_lines = width;
	  granularity = 4;
	  break;

	case 6:					/* circle wipe */
	  {
	    int full = 360 * 64;
	    int inc = full / 64;
	    int start = random() % full;
	    int rad = (width > height ? width : height);
	    if (random() & 1)
	      inc = -inc;
	    for (i = (inc > 0 ? 0 : full);
		 (inc > 0 ? i < full : i > 0);
		 i += inc) {
	      XFillArc(dpy, window, gc,
		       (width/2)-rad, (height/2)-rad, rad*2, rad*2,
		       (i+start) % full, inc);
	      XFlush (dpy);
	      usleep (delay*granularity);
	    }
	  num_lines = 0;
	  }
	  break;

	case 7:					/* three-circle wipe */
	  {
	    int full = 360 * 64;
	    int q = full / 3;
	    int inc = full / 180;
	    int start = random() % q;
	    int rad = (width > height ? width : height);
	    if (random() & 1)
	      inc = -inc;
	    for (i = (inc > 0 ? 0 : q);
		 (inc > 0 ? i < q : i > 0);
		 i += inc) {
	      XFillArc(dpy, window, gc,
		       (width/2)-rad, (height/2)-rad, rad*2, rad*2,
		       (i+start) % full, inc);
	      XFillArc(dpy, window, gc,
		       (width/2)-rad, (height/2)-rad, rad*2, rad*2,
		       (i+start+q) % full, inc);
	      XFillArc(dpy, window, gc,
		       (width/2)-rad, (height/2)-rad, rad*2, rad*2,
		       (i+start+q+q) % full, inc);
	      XFlush (dpy);
	      usleep (delay*granularity);
	    }
	  num_lines = 0;
	  }
	  break;

	default:
	  abort();
	  break;
	}

      for (i = 0; i < num_lines; i++)
	{ 
	  if(clear_lines[i] < height)
	    XDrawLine (dpy, window, gc, 0, clear_lines[i], width, 
		       clear_lines[i]);
	  else
	    XDrawLine (dpy, window, gc, clear_lines[i]-height, 0,
		       clear_lines[i]-height, height);
	  XFlush (dpy);
	  if ((i % granularity) == 0)
	    {
	      usleep (delay*granularity);
	    }
	}
      
      free(clear_lines);
    }

  XClearWindow (dpy, window);
  XSync(dpy, False);
}


void
erase_full_window(Display *dpy, Window window)
{
  XWindowAttributes xgwa;
  XGCValues gcv;
  GC erase_gc;
  XColor black;
  int erase_speed = get_integer_resource("eraseSpeed", "Integer");
  int erase_mode = get_integer_resource("eraseMode", "Integer");
  XGetWindowAttributes (dpy, window, &xgwa);
  black.flags = DoRed|DoGreen|DoBlue;
  black.red = black.green = black.blue = 0;
  XAllocColor(dpy, xgwa.colormap, &black);
  gcv.foreground = black.pixel;
  erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
  erase_window (dpy, window, erase_gc, xgwa.width, xgwa.height,
		erase_mode, erase_speed);
  XFreeColors(dpy, xgwa.colormap, &black.pixel, 1, 0);
  XFreeGC(dpy, erase_gc);
}
