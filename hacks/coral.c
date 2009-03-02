/* coral, by "Frederick G.M. Roeber" <roeber@netscape.com>, 15-jul-97.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "screenhack.h"
#include "colors.h"
#include "erase.h"

#define NCOLORSMAX 200

struct state {
  Display *dpy;
  Window window;

   GC draw_gc, erase_gc;
   unsigned int default_fg_pixel;
   XColor colors[NCOLORSMAX];
   int ncolors;
   int colorindex;
   int colorsloth;

   XPoint *walkers;
   int nwalkers;
   int width, widthb;
   int height;
   int delay, delay2;
   int max_points;
   XPoint *pointbuf;

   unsigned int *board;

   int done, reset;
   int npoints;
   eraser_state *eraser;
};


#define getdot(x,y) (st->board[(y*st->widthb)+(x>>5)] &  (1<<(x & 31)))
#define setdot(x,y) (st->board[(y*st->widthb)+(x>>5)] |= (1<<(x & 31)))


static void
init_coral(struct state *st)
{
    XGCValues gcv;
    Colormap cmap;
    XWindowAttributes xgwa;
    Bool writeable = False;
    int seeds;
    int density;
    int i;

    XClearWindow(st->dpy, st->window);
    XGetWindowAttributes(st->dpy, st->window, &xgwa);
    st->width = xgwa.width;
    st->widthb = ((xgwa.width + 31) >> 5);
    st->height = xgwa.height;
    if (st->board) free(st->board);
    st->board = (unsigned int *)calloc(st->widthb * xgwa.height, sizeof(unsigned int));
    if(!st->board) exit(1);
    cmap = xgwa.colormap;
    if( st->ncolors ) {
        free_colors(st->dpy, cmap, st->colors, st->ncolors);
        st->ncolors = 0;
    }
    gcv.foreground = st->default_fg_pixel = get_pixel_resource(st->dpy, cmap, "foreground", "Foreground");
    st->draw_gc = XCreateGC(st->dpy, st->window, GCForeground, &gcv);
    gcv.foreground = get_pixel_resource (st->dpy, cmap, "background", "Background");
    st->erase_gc = XCreateGC (st->dpy, st->window, GCForeground, &gcv);
    st->ncolors = NCOLORSMAX;
    make_uniform_colormap(st->dpy, xgwa.visual, cmap, st->colors, &st->ncolors, True, &writeable, False);
    if (st->ncolors <= 0) {
      st->ncolors = 2;
      st->colors[0].red = st->colors[0].green = st->colors[0].blue = 0;
      st->colors[1].red = st->colors[1].green = st->colors[1].blue = 0xFFFF;
      XAllocColor(st->dpy, cmap, &st->colors[0]);
      XAllocColor(st->dpy, cmap, &st->colors[1]);
   }
    st->colorindex = random()%st->ncolors;
    
    density = get_integer_resource(st->dpy, "density", "Integer");
    if( density < 1 ) density = 1;
    if( density > 100 ) density = 90; /* more like mold than coral */
    st->nwalkers = (st->width*st->height*density)/100;
    if (st->walkers) free(st->walkers);
    st->walkers = (XPoint *)calloc(st->nwalkers, sizeof(XPoint));
    if( (XPoint *)0 == st->walkers ) exit(1);

    seeds = get_integer_resource(st->dpy, "seeds", "Integer");
    if( seeds < 1 ) seeds = 1;
    if( seeds > 1000 ) seeds = 1000;

    st->colorsloth = st->nwalkers*2/st->ncolors;
    XSetForeground(st->dpy, st->draw_gc, st->colors[st->colorindex].pixel);

    if ((st->width <= 2) || (st->height <= 2)) return;

    for( i = 0; i < seeds; i++ ) {
        int x, y;
	int max_repeat = 10;
        do {
          x = 1 + random() % (st->width - 2);
          y = 1 + random() % (st->height - 2);
        } while( getdot(x, y) && max_repeat--);

        setdot((x-1), (y-1)); setdot(x, (y-1)); setdot((x+1), (y-1));
	setdot((x-1),  y   ); setdot(x,  y   ); setdot((x+1),  y   );
	setdot((x-1), (y+1)); setdot(x, (y+1)); setdot((x+1), (y+1));
        XDrawPoint(st->dpy, st->window, st->draw_gc, x, y);
    }

    for( i = 0; i < st->nwalkers; i++ ) {
        st->walkers[i].x = (random() % (st->width-2)) + 1;
        st->walkers[i].y = (random() % (st->height-2)) + 1;
    }
}


/* returns 2 bits of randomness (conserving calls to random()).
   This speeds things up a little, but not a lot (5-10% or so.)
 */
static int 
rand_2(void)
{
  static int i = 0;
  static int r = 0;
  if (i != 0) {
    i--;
  } else {
    i = 15;
    r = random();
  }

  {
    register int j = (r & 3);
    r = r >> 2;
    return j;
  }
}


static int
coral(struct state *st)
{
  int i = 0;

  for( i = 0; i < st->nwalkers; i++ ) {
    int x = st->walkers[i].x;
    int y = st->walkers[i].y;

    if( getdot(x, y) ) {

      Bool flush = False;
      Bool color = False;

      /* XDrawPoint(dpy, window, draw_gc, x, y); */
      st->pointbuf[st->npoints].x = x;
      st->pointbuf[st->npoints].y = y;
      st->npoints++;

      /* Mark the surrounding area as "sticky" */
      setdot((x-1), (y-1)); setdot(x, (y-1)); setdot((x+1), (y-1));
      setdot((x-1),  y   );                   setdot((x+1),  y   );
      setdot((x-1), (y+1)); setdot(x, (y+1)); setdot((x+1), (y+1));
      st->nwalkers--;
      st->walkers[i].x = st->walkers[st->nwalkers].x;
      st->walkers[i].y = st->walkers[st->nwalkers].y;
      if( 0 == 
	  ((st->colorsloth ? st->nwalkers%st->colorsloth : 0)) ) {
        color = True;
      }
		  
      if (flush || color || 0 == st->nwalkers || st->npoints >= st->max_points) {
        XDrawPoints(st->dpy, st->window, st->draw_gc, st->pointbuf, st->npoints,
                    CoordModeOrigin);
        st->npoints = 0;
      }

      if (color) {
        st->colorindex++;
        if( st->colorindex == st->ncolors )
          st->colorindex = 0;
        XSetForeground(st->dpy, st->draw_gc, st->colors[st->colorindex].pixel);
      }
    } else {
      /* move it a notch */
      do {
        switch(rand_2()) {
        case 0:
          if( 1 == x ) continue;
          st->walkers[i].x--;
          break;
        case 1:
          if( st->width-2 == x ) continue;
          st->walkers[i].x++;
          break;
        case 2:
          if( 1 == y ) continue;
          st->walkers[i].y--;
          break;
        default: /* case 3: */
          if( st->height-2 == y ) continue;
          st->walkers[i].y++;
          break;
          /* default:
             abort(); */
        }
      } while(0);
    }
  }

 return (0 == st->nwalkers);
}

static void *
coral_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;
  st->max_points = 200;
  st->pointbuf = (XPoint *) calloc(sizeof(XPoint), st->max_points+2);
  if (!st->pointbuf) exit(-1);

  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->delay2 = get_integer_resource (st->dpy, "delay2", "Integer");
  st->reset = 1;
  return st;
}


static unsigned long
coral_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->eraser || st->done)
    {
      st->done = 0;
      st->eraser = erase_window (st->dpy, st->window, st->eraser);
      return st->delay2;
    }

  if (st->reset)
    init_coral(st);
  st->reset = st->done = coral(st);

  return (st->reset
          ? (st->delay * 1000000)
          : st->delay2);
}

static void
coral_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
coral_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
coral_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st->pointbuf);
  if (st->walkers) free (st->walkers);
  if (st->board) free (st->board);
  free (st);
}

static const char *coral_defaults[] = {
  ".background:	black",
  ".foreground:	white",
  "*density:	25",
  "*seeds:	20", /* too many for 640x480, too few for 1280x1024 */
  "*delay:	5",
  "*delay2:	20000",
  0
};

static XrmOptionDescRec coral_options[] = {
    { "-density", ".density", XrmoptionSepArg, 0 },
    { "-seeds", ".seeds", XrmoptionSepArg, 0 },
    { "-delay", ".delay", XrmoptionSepArg, 0 },
    { "-delay2", ".delay2", XrmoptionSepArg, 0 },
    { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Coral", coral)
