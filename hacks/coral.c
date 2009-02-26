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

static GC draw_gc, erase_gc;
static unsigned int default_fg_pixel;
#define NCOLORSMAX 200
static XColor colors[NCOLORSMAX];
static int ncolors = 0;
static int colorindex = 0;
static int colorsloth;

static XPoint *walkers = 0;
static int nwalkers;
static int width, widthb;
static int height;

static unsigned int *board = 0;
#define getdot(x,y) (board[(y*widthb)+(x>>5)] &  (1<<(x & 31)))
#define setdot(x,y) (board[(y*widthb)+(x>>5)] |= (1<<(x & 31)))


static void
init_coral(Display *dpy, Window window)
{
    XGCValues gcv;
    Colormap cmap;
    XWindowAttributes xgwa;
    Bool writeable = False;
    int seeds;
    int density;
    int i;

    XClearWindow(dpy, window);
    XGetWindowAttributes(dpy, window, &xgwa);
    width = xgwa.width;
    widthb = ((xgwa.width + 31) >> 5);
    height = xgwa.height;
    if (board) free(board);
    board = (unsigned int *)calloc(widthb * xgwa.height, sizeof(unsigned int));
    if(!board) exit(1);
    cmap = xgwa.colormap;
    if( ncolors ) {
        free_colors(dpy, cmap, colors, ncolors);
        ncolors = 0;
    }
    gcv.foreground = default_fg_pixel = get_pixel_resource("foreground", "Foreground", dpy, cmap);
    draw_gc = XCreateGC(dpy, window, GCForeground, &gcv);
    gcv.foreground = get_pixel_resource ("background", "Background",dpy, cmap);
    erase_gc = XCreateGC (dpy, window, GCForeground, &gcv);
    ncolors = NCOLORSMAX;
    make_uniform_colormap(dpy, xgwa.visual, cmap, colors, &ncolors, True, &writeable, False);
    if (ncolors <= 0) {
      ncolors = 2;
      colors[0].red = colors[0].green = colors[0].blue = 0;
      colors[1].red = colors[1].green = colors[1].blue = 0xFFFF;
      XAllocColor(dpy, cmap, &colors[0]);
      XAllocColor(dpy, cmap, &colors[1]);
   }
    colorindex = random()%ncolors;
    
    density = get_integer_resource("density", "Integer");
    if( density < 1 ) density = 1;
    if( density > 100 ) density = 90; /* more like mold than coral */
    nwalkers = (width*height*density)/100;
    if (walkers) free(walkers);
    walkers = (XPoint *)calloc(nwalkers, sizeof(XPoint));
    if( (XPoint *)0 == walkers ) exit(1);

    seeds = get_integer_resource("seeds", "Integer");
    if( seeds < 1 ) seeds = 1;
    if( seeds > 1000 ) seeds = 1000;

    colorsloth = nwalkers*2/ncolors;
    XSetForeground(dpy, draw_gc, colors[colorindex].pixel);

    for( i = 0; i < seeds; i++ ) {
        int x, y;
        do {
            x = random() % width;
            y = random() % height;
        } while( getdot(x, y) );

        setdot((x-1), (y-1)); setdot(x, (y-1)); setdot((x+1), (y-1));
	setdot((x-1),  y   ); setdot(x,  y   ); setdot((x+1),  y   );
	setdot((x-1), (y+1)); setdot(x, (y+1)); setdot((x+1), (y+1));
        XDrawPoint(dpy, window, draw_gc, x, y);
    }

    for( i = 0; i < nwalkers; i++ ) {
        walkers[i].x = (random() % (width-2)) + 1;
        walkers[i].y = (random() % (height-2)) + 1;
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


static void
coral(Display *dpy, Window window)
{
    int delay2 = get_integer_resource ("delay2", "Integer");

    int max_points = 200;
    int npoints = 0;
    XPoint *pointbuf = (XPoint *) calloc(sizeof(XPoint), max_points+2);
    if (!pointbuf) exit(-1);

    while( 1 ) {
        int i;

        for( i = 0; i < nwalkers; i++ ) {
            int x = walkers[i].x;
            int y = walkers[i].y;

            if( getdot(x, y) ) {

	        Bool flush = False;
	        Bool color = False;

	        /* XDrawPoint(dpy, window, draw_gc, x, y); */
	        pointbuf[npoints].x = x;
	        pointbuf[npoints].y = y;
		npoints++;

                /* Mark the surrounding area as "sticky" */
                setdot((x-1), (y-1)); setdot(x, (y-1)); setdot((x+1), (y-1));
		setdot((x-1),  y   );                   setdot((x+1),  y   );
                setdot((x-1), (y+1)); setdot(x, (y+1)); setdot((x+1), (y+1));
                nwalkers--;
                walkers[i].x = walkers[nwalkers].x;
                walkers[i].y = walkers[nwalkers].y;
                if( 0 == (nwalkers%colorsloth) ) {
		  color = True;
                }
		  
		if (flush || color || 0 == nwalkers || npoints >= max_points) {
		  XDrawPoints(dpy, window, draw_gc, pointbuf, npoints,
			      CoordModeOrigin);
		  npoints = 0;
		  XSync(dpy, True);
		}

		if (color) {
                    colorindex++;
                    if( colorindex == ncolors )
                        colorindex = 0;
                    XSetForeground(dpy, draw_gc, colors[colorindex].pixel);
                }

                if( 0 == nwalkers ) {
                    XSync(dpy, True);
		    free(pointbuf);
                    return;
                }
            } else {
                /* move it a notch */
                do {
                    switch(rand_2()) {
                    case 0:
                        if( 1 == x ) continue;
                        walkers[i].x--;
                        break;
                    case 1:
                        if( width-2 == x ) continue;
                        walkers[i].x++;
                        break;
                    case 2:
                        if( 1 == y ) continue;
                        walkers[i].y--;
                        break;
                    default: /* case 3: */
                        if( height-2 == y ) continue;
                        walkers[i].y++;
                        break;
		    /* default:
		      abort(); */
                    }
                } while(0);
            }
        }

	if (delay2 > 0) {
	  if (npoints > 0) {
	    XDrawPoints(dpy, window, draw_gc, pointbuf, npoints,
			CoordModeOrigin);
	    npoints = 0;
	    XSync(dpy, True);
	  }
	  usleep(delay2);
	}
    }
}

char *progclass = "Coral";

char *defaults[] = {
  ".background:	black",
  ".foreground:	white",
  "*density:	25",
  "*seeds:	20", /* too many for 640x480, too few for 1280x1024 */
  "*delay:	5",
  "*delay2:	1000",
  0
};

XrmOptionDescRec options[] = {
    { "-density", ".density", XrmoptionSepArg, 0 },
    { "-seeds", ".seeds", XrmoptionSepArg, 0 },
    { "-delay", ".delay", XrmoptionSepArg, 0 },
    { "-delay2", ".delay2", XrmoptionSepArg, 0 },
    { 0, 0, 0, 0 }
};

void
screenhack(dpy, window)
Display *dpy;
Window window;
{
    int delay = get_integer_resource ("delay", "Integer");
    while( 1 ) {
        init_coral(dpy, window);
        coral(dpy, window);
        if( delay ) sleep(delay);
	erase_full_window(dpy, window);
    }
}
