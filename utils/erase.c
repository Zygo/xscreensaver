/* erase.c: Erase the screen in various more or less interesting ways.
 * (c) 1997 by Johannes Keukelaar <johannes@nada.kth.se>
 * Permission to use in any way granted. Provided "as is" without expressed
 * or implied warranty. NO WARRANTY, NO EXPRESSION OF SUITABILITY FOR ANY
 * PURPOSE. (I.e.: Use in any way, but at your own risk!)
 */

#include "utils.h"
#include "yarandom.h"
#include "usleep.h"

#define NUM_MODES 6

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
	case 0:
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
	case 1:
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
	case 2:
	  for(i = 0; i < width/2; i++)
	    clear_lines[i] = i*2+height;
	  for(i = 0; i < height/2; i++)
	    clear_lines[i+width/2] = i*2;
	  for(i = 0; i < width/2; i++)
	    clear_lines[i+width/2+height/2] = width-i*2-(width%2?0:1)+height;
	  num_lines = width+height/2;
	  granularity = 4;
	  break;
	case 3:
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
	case 4:
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
	case 5:
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
}
