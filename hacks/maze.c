/******************************************************************************
 * [ maze ] ...
 *
 * modified:  [ 3-7-93 ]  Jamie Zawinski <jwz@mcom.com>
 *		added the XRoger logo, cleaned up resources, made
 *		grid size a parameter.
 * modified:  [ 3-3-93 ]  Jim Randell <jmr@mddjmr.fc.hp.com>
 *		Added the colour stuff and integrated it with jwz's
 *		screenhack stuff.  There's still some work that could
 *		be done on this, particularly allowing a resource to
 *		specify how big the squares are.
 * modified:  [ 10-4-88 ]  Richard Hess    ...!uunet!cimshop!rhess  
 *              [ Revised primary execution loop within main()...
 *              [ Extended X event handler, check_events()...
 * modified:  [ 1-29-88 ]  Dave Lemke      lemke@sun.com  
 *              [ Hacked for X11...
 *              [  Note the word "hacked" -- this is extremely ugly, but at 
 *              [   least it does the job.  NOT a good programming example 
 *              [   for X.
 * original:  [ 6/21/85 ]  Martin Weiss    Sun Microsystems  [ SunView ]
 *
 ******************************************************************************
 Copyright 1988 by Sun Microsystems, Inc. Mountain View, CA.
  
 All Rights Reserved
  
 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted, 
 provided that the above copyright notice appear in all copies and that
 both that copyright notice and this permission notice appear in 
 supporting documentation, and that the names of Sun or MIT not be
 used in advertising or publicity pertaining to distribution of the
 software without specific prior written permission. Sun and M.I.T. 
 make no representations about the suitability of this software for 
 any purpose. It is provided "as is" without any express or implied warranty.
 
 SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE. IN NO EVENT SHALL SUN BE LIABLE FOR ANY SPECIAL, INDIRECT
 OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

#include "screenhack.h"

#define XROGER

static int solve_delay, pre_solve_delay, post_solve_delay;

#include  <stdio.h>
#include  <X11/Xlib.h>
#include  <X11/Xutil.h>
#ifndef VMS
#include  <X11/bitmaps/gray1>
#else
#include "sys$common:[decw$include.bitmaps]gray1.xbm"
#endif

#define MAX_MAZE_SIZE_X	500
#define MAX_MAZE_SIZE_Y	500

#define MOVE_LIST_SIZE  (MAX_MAZE_SIZE_X * MAX_MAZE_SIZE_Y)

#define WALL_TOP	0x8000
#define WALL_RIGHT	0x4000
#define WALL_BOTTOM	0x2000
#define WALL_LEFT	0x1000

#define DOOR_IN_TOP	0x800
#define DOOR_IN_RIGHT	0x400
#define DOOR_IN_BOTTOM	0x200
#define DOOR_IN_LEFT	0x100
#define DOOR_IN_ANY	0xF00

#define DOOR_OUT_TOP	0x80
#define DOOR_OUT_RIGHT	0x40
#define DOOR_OUT_BOTTOM	0x20
#define DOOR_OUT_LEFT	0x10

#define START_SQUARE	0x2
#define END_SQUARE	0x1

#define	border_x        (0)
#define	border_y        (0)

#define	get_random(x)	(random() % (x))
  
static int logo_x, logo_y;

#ifdef XROGER
# define logo_width  128
# define logo_height 128
#else
#ifndef VMS
# include  <X11/bitmaps/xlogo64>
#else
# include "sys$common:[decw$include.bitmaps]xlogo64.xbm"
#endif
# define logo_width xlogo64_width
# define logo_height xlogo64_height
# define logo_bits xlogo64_bits
#endif

static unsigned short maze[MAX_MAZE_SIZE_X][MAX_MAZE_SIZE_Y];

static struct {
  unsigned char x;
  unsigned char y;
  unsigned char dir;
} move_list[MOVE_LIST_SIZE], save_path[MOVE_LIST_SIZE], path[MOVE_LIST_SIZE];

static int maze_size_x, maze_size_y;
static int sqnum, cur_sq_x, cur_sq_y, path_length;
static int start_x, start_y, start_dir, end_x, end_y, end_dir;
static int grid_width, grid_height;

static Display	*dpy;
static Window	win;
static GC	gc, cgc, tgc, logo_gc;
static Pixmap	logo_map;

static int	x = 0, y = 0, restart = 0, stop = 1, state = 1;

static int
check_events()                                  /* X event handler [ rhess ] */
{
  XEvent	e;

  if (XPending(dpy)) {
    XNextEvent(dpy, &e);
    switch (e.type) {

    case ButtonPress:
      switch (e.xbutton.button) {
      case 3:
	exit (0);
	break;
      case 2:
	stop = !stop ;
	if (state == 5) state = 4 ;
	else {
	  restart = 1;
	  stop = 0;
	}
	break;
      default:
	restart = 1 ;
	stop = 0 ;
	break;
      }
      break;

    case ConfigureNotify:
      restart = 1;
      break;
    case UnmapNotify:
      stop = 1;
      XClearWindow (dpy, win);
      XSync (dpy, False);
      break;
    case Expose:
      restart = 1;
      break;
    }
    return(1);
  }
  return(0);
}


static void
set_maze_sizes (width, height)
     int width, height;
{
  maze_size_x = width / grid_width;
  maze_size_y = height / grid_height;
}


static void
initialize_maze()         /* draw the surrounding wall and start/end squares */
{
  register int i, j, wall;
  
  /* initialize all squares */
  for ( i=0; i<maze_size_x; i++) {
    for ( j=0; j<maze_size_y; j++) {
      maze[i][j] = 0;
    }
  }
  
  /* top wall */
  for ( i=0; i<maze_size_x; i++ ) {
    maze[i][0] |= WALL_TOP;
  }
  
  /* right wall */
  for ( j=0; j<maze_size_y; j++ ) {
    maze[maze_size_x-1][j] |= WALL_RIGHT;
  }
  
  /* bottom wall */
  for ( i=0; i<maze_size_x; i++ ) {
    maze[i][maze_size_y-1] |= WALL_BOTTOM;
  }
  
  /* left wall */
  for ( j=0; j<maze_size_y; j++ ) {
    maze[0][j] |= WALL_LEFT;
  }
  
  /* set start square */
  wall = get_random(4);
  switch (wall) {
  case 0:	
    i = get_random(maze_size_x);
    j = 0;
    break;
  case 1:	
    i = maze_size_x - 1;
    j = get_random(maze_size_y);
    break;
  case 2:	
    i = get_random(maze_size_x);
    j = maze_size_y - 1;
    break;
  case 3:	
    i = 0;
    j = get_random(maze_size_y);
    break;
  }
  maze[i][j] |= START_SQUARE;
  maze[i][j] |= ( DOOR_IN_TOP >> wall );
  maze[i][j] &= ~( WALL_TOP >> wall );
  cur_sq_x = i;
  cur_sq_y = j;
  start_x = i;
  start_y = j;
  start_dir = wall;
  sqnum = 0;
  
  /* set end square */
  wall = (wall + 2)%4;
  switch (wall) {
  case 0:
    i = get_random(maze_size_x);
    j = 0;
    break;
  case 1:
    i = maze_size_x - 1;
    j = get_random(maze_size_y);
    break;
  case 2:
    i = get_random(maze_size_x);
    j = maze_size_y - 1;
    break;
  case 3:
    i = 0;
    j = get_random(maze_size_y);
    break;
  }
  maze[i][j] |= END_SQUARE;
  maze[i][j] |= ( DOOR_OUT_TOP >> wall );
  maze[i][j] &= ~( WALL_TOP >> wall );
  end_x = i;
  end_y = j;
  end_dir = wall;
  
  /* set logo */
  if ((maze_size_x > 15) && (maze_size_y > 15))
    {
      int logow = 1 + logo_width / grid_width;
      int logoh = 1 + logo_height / grid_height;
      /* not closer than 3 grid units from a wall */
      logo_x = get_random (maze_size_x - logow - 6) + 3;
      logo_y = get_random (maze_size_y - logoh - 6) + 3;
      for (i=0; i<logow; i++)
	for (j=0; j<logoh; j++)
	  maze[logo_x + i][logo_y + j] |= DOOR_IN_TOP;
    }
  else
    logo_y = logo_x = -1;
}

static int choose_door ();
static int backup ();
static void draw_wall ();
static void draw_solid_square ();
static void enter_square ();

static void
create_maze()             /* create a maze layout given the intiialized maze */
{
  register int i, newdoor;
  
  do {
    move_list[sqnum].x = cur_sq_x;
    move_list[sqnum].y = cur_sq_y;
    move_list[sqnum].dir = newdoor;
    while ( ( newdoor = choose_door() ) == -1 ) { /* pick a door */
      if ( backup() == -1 ) { /* no more doors ... backup */
	return; /* done ... return */
      }
    }
    
    /* mark the out door */
    maze[cur_sq_x][cur_sq_y] |= ( DOOR_OUT_TOP >> newdoor );
    
    switch (newdoor) {
    case 0: cur_sq_y--;
      break;
    case 1: cur_sq_x++;
      break;
    case 2: cur_sq_y++;
      break;
    case 3: cur_sq_x--;
      break;
    }
    sqnum++;
    
    /* mark the in door */
    maze[cur_sq_x][cur_sq_y] |= ( DOOR_IN_TOP >> ((newdoor+2)%4) );
    
    /* if end square set path length and save path */
    if ( maze[cur_sq_x][cur_sq_y] & END_SQUARE ) {
      path_length = sqnum;
      for ( i=0; i<path_length; i++) {
	save_path[i].x = move_list[i].x;
	save_path[i].y = move_list[i].y;
	save_path[i].dir = move_list[i].dir;
      }
    }
    
  } while (1);
  
}


static int
choose_door()                                            /* pick a new path */
{
  int candidates[3];
  register int num_candidates;
  
  num_candidates = 0;
  
  /* top wall */
  if ( maze[cur_sq_x][cur_sq_y] & DOOR_IN_TOP )
    goto rightwall;
  if ( maze[cur_sq_x][cur_sq_y] & DOOR_OUT_TOP )
    goto rightwall;
  if ( maze[cur_sq_x][cur_sq_y] & WALL_TOP )
    goto rightwall;
  if ( maze[cur_sq_x][cur_sq_y - 1] & DOOR_IN_ANY ) {
    maze[cur_sq_x][cur_sq_y] |= WALL_TOP;
    maze[cur_sq_x][cur_sq_y - 1] |= WALL_BOTTOM;
    draw_wall(cur_sq_x, cur_sq_y, 0);
    goto rightwall;
  }
  candidates[num_candidates++] = 0;
  
 rightwall:
  /* right wall */
  if ( maze[cur_sq_x][cur_sq_y] & DOOR_IN_RIGHT )
    goto bottomwall;
  if ( maze[cur_sq_x][cur_sq_y] & DOOR_OUT_RIGHT )
    goto bottomwall;
  if ( maze[cur_sq_x][cur_sq_y] & WALL_RIGHT )
    goto bottomwall;
  if ( maze[cur_sq_x + 1][cur_sq_y] & DOOR_IN_ANY ) {
    maze[cur_sq_x][cur_sq_y] |= WALL_RIGHT;
    maze[cur_sq_x + 1][cur_sq_y] |= WALL_LEFT;
    draw_wall(cur_sq_x, cur_sq_y, 1);
    goto bottomwall;
  }
  candidates[num_candidates++] = 1;
  
 bottomwall:
  /* bottom wall */
  if ( maze[cur_sq_x][cur_sq_y] & DOOR_IN_BOTTOM )
    goto leftwall;
  if ( maze[cur_sq_x][cur_sq_y] & DOOR_OUT_BOTTOM )
    goto leftwall;
  if ( maze[cur_sq_x][cur_sq_y] & WALL_BOTTOM )
    goto leftwall;
  if ( maze[cur_sq_x][cur_sq_y + 1] & DOOR_IN_ANY ) {
    maze[cur_sq_x][cur_sq_y] |= WALL_BOTTOM;
    maze[cur_sq_x][cur_sq_y + 1] |= WALL_TOP;
    draw_wall(cur_sq_x, cur_sq_y, 2);
    goto leftwall;
  }
  candidates[num_candidates++] = 2;
  
 leftwall:
  /* left wall */
  if ( maze[cur_sq_x][cur_sq_y] & DOOR_IN_LEFT )
    goto donewall;
  if ( maze[cur_sq_x][cur_sq_y] & DOOR_OUT_LEFT )
    goto donewall;
  if ( maze[cur_sq_x][cur_sq_y] & WALL_LEFT )
    goto donewall;
  if ( maze[cur_sq_x - 1][cur_sq_y] & DOOR_IN_ANY ) {
    maze[cur_sq_x][cur_sq_y] |= WALL_LEFT;
    maze[cur_sq_x - 1][cur_sq_y] |= WALL_RIGHT;
    draw_wall(cur_sq_x, cur_sq_y, 3);
    goto donewall;
  }
  candidates[num_candidates++] = 3;
  
 donewall:
  if (num_candidates == 0)
    return ( -1 );
  if (num_candidates == 1)
    return ( candidates[0] );
  return ( candidates[ get_random(num_candidates) ] );
  
}


static int
backup()                                                  /* back up a move */
{
  sqnum--;
  cur_sq_x = move_list[sqnum].x;
  cur_sq_y = move_list[sqnum].y;
  return ( sqnum );
}


static void
draw_maze_border()                                  /* draw the maze outline */
{
  register int i, j;
  
  
  for ( i=0; i<maze_size_x; i++) {
    if ( maze[i][0] & WALL_TOP ) {
      XDrawLine(dpy, win, gc,
		border_x + grid_width * i,
		border_y,
		border_x + grid_width * (i+1) - 1,
		border_y);
    }
    if ((maze[i][maze_size_y - 1] & WALL_BOTTOM)) {
      XDrawLine(dpy, win, gc,
		border_x + grid_width * i,
		border_y + grid_height * (maze_size_y) - 1,
		border_x + grid_width * (i+1) - 1,
		border_y + grid_height * (maze_size_y) - 1);
    }
  }
  for ( j=0; j<maze_size_y; j++) {
    if ( maze[maze_size_x - 1][j] & WALL_RIGHT ) {
      XDrawLine(dpy, win, gc,
		border_x + grid_width * maze_size_x - 1,
		border_y + grid_height * j,
		border_x + grid_width * maze_size_x - 1,
		border_y + grid_height * (j+1) - 1);
    }
    if ( maze[0][j] & WALL_LEFT ) {
      XDrawLine(dpy, win, gc,
		border_x,
		border_y + grid_height * j,
		border_x,
		border_y + grid_height * (j+1) - 1);
    }
  }
  
  if (logo_x != -1)
    {
      Window r;
      int x, y;
      unsigned int w, h, bw, d;
      XGetGeometry (dpy, logo_map, &r, &x, &y, &w, &h, &bw, &d);
      XCopyPlane (dpy, logo_map, win, logo_gc,
		  0, 0, w, h,
		  border_x + 3 + grid_width * logo_x,
		  border_y + 3 + grid_height * logo_y, 1);
    }
  draw_solid_square (start_x, start_y, start_dir, tgc);
  draw_solid_square (end_x, end_y, end_dir, tgc);
}


static void
draw_wall(i, j, dir)                                   /* draw a single wall */
     int i, j, dir;
{
  switch (dir) {
  case 0:
    XDrawLine(dpy, win, gc,
	      border_x + grid_width * i, 
	      border_y + grid_height * j,
	      border_x + grid_width * (i+1), 
	      border_y + grid_height * j);
    break;
  case 1:
    XDrawLine(dpy, win, gc,
	      border_x + grid_width * (i+1), 
	      border_y + grid_height * j,
	      border_x + grid_width * (i+1), 
	      border_y + grid_height * (j+1));
    break;
  case 2:
    XDrawLine(dpy, win, gc,
	      border_x + grid_width * i, 
	      border_y + grid_height * (j+1),
	      border_x + grid_width * (i+1), 
	      border_y + grid_height * (j+1));
    break;
  case 3:
    XDrawLine(dpy, win, gc,
	      border_x + grid_width * i, 
	      border_y + grid_height * j,
	      border_x + grid_width * i, 
	      border_y + grid_height * (j+1));
    break;
  }
}

int bw;

static void
draw_solid_square(i, j, dir, gc)          /* draw a solid square in a square */
     register int i, j, dir;
     GC	gc;
{
  switch (dir) {
  case 0: XFillRectangle(dpy, win, gc,
			 border_x + bw + grid_width * i, 
			 border_y - bw + grid_height * j, 
			 grid_width - (bw+bw), grid_height);
    break;
  case 1: XFillRectangle(dpy, win, gc,
			 border_x + bw + grid_width * i, 
			 border_y + bw + grid_height * j, 
			 grid_width, grid_height - (bw+bw));
    break;
  case 2: XFillRectangle(dpy, win, gc,
			 border_x + bw + grid_width * i, 
			 border_y + bw + grid_height * j, 
			 grid_width - (bw+bw), grid_height);
    break;
  case 3: XFillRectangle(dpy, win, gc,
			 border_x - bw + grid_width * i, 
			 border_y + bw + grid_height * j, 
			 grid_width, grid_height - (bw+bw));
    break;
  }
  XSync (dpy, False);
}


static void
solve_maze()                             /* solve it with graphical feedback */
{
  int i;
  
  
  /* plug up the surrounding wall */
  maze[start_x][start_y] |= (WALL_TOP >> start_dir);
  maze[end_x][end_y] |= (WALL_TOP >> end_dir);
  
  /* initialize search path */
  i = 0;
  path[i].x = end_x;
  path[i].y = end_y;
  path[i].dir = -1;
  
  /* do it */
  while (1) {
    if ( ++path[i].dir >= 4 ) {
      i--;
      draw_solid_square( (int)(path[i].x), (int)(path[i].y), 
			(int)(path[i].dir), cgc);
    }
    else if ( ! (maze[path[i].x][path[i].y] & 
		 (WALL_TOP >> path[i].dir))  && 
	     ( (i == 0) || ( (path[i].dir != 
			      (int)(path[i-1].dir+2)%4) ) ) ) {
      enter_square(i);
      i++;
      if ( maze[path[i].x][path[i].y] & START_SQUARE ) {
	return;
      }
    } 
    if (check_events()) return;
    /* Abort solve on expose - cheapo repaint strategy */
    if (solve_delay) usleep (solve_delay);
  }
} 


static void
enter_square(n)                            /* move into a neighboring square */
     int n;
{
  draw_solid_square( (int)path[n].x, (int)path[n].y, 
		    (int)path[n].dir, tgc);
  
  path[n+1].dir = -1;
  switch (path[n].dir) {
  case 0: path[n+1].x = path[n].x;
    path[n+1].y = path[n].y - 1;
    break;
  case 1: path[n+1].x = path[n].x + 1;
    path[n+1].y = path[n].y;
    break;
  case 2: path[n+1].x = path[n].x;
    path[n+1].y = path[n].y + 1;
    break;
  case 3: path[n+1].x = path[n].x - 1;
    path[n+1].y = path[n].y;
    break;
  }
}

/* ----<eof> */


/*
 *  jmr additions for Jamie Zawinski's <jwz@mcom.com> screensaver stuff,
 *  note that the code above this has probably been hacked about in some
 *  arbitrary way.
 */

char *progclass = "Maze";

char *defaults[] = {
  "Maze.background:	black",		/* to placate SGI */
  "Maze.foreground:	white",		/* to placate SGI */
  "*gridSize:	0",
  "*solveDelay:	5000",
  "*preDelay:	2000000",
  "*postDelay:	4000000",
  "*liveColor:	green",
  "*deadColor:	red",
#ifdef XROGER
  "*logoColor:	red3",
#endif
  0
};

XrmOptionDescRec options[] = {
  { "-grid-size",	".gridSize",	XrmoptionSepArg, 0 },
  { "-solve-delay",	".solveDelay",	XrmoptionSepArg, 0 },
  { "-pre-delay",	".preDelay",	XrmoptionSepArg, 0 },
  { "-post-delay",	".postDelay",	XrmoptionSepArg, 0 },
  { "-live-color",	".liveColor",	XrmoptionSepArg, 0 },
  { "-dead-color",	".deadColor",	XrmoptionSepArg, 0 }
};

int options_size = (sizeof(options)/sizeof(options[0]));

void screenhack(display,window)
     Display *display;
     Window window;
{
  Pixmap gray;
  int size, root;
  XWindowAttributes xgwa;
  unsigned long bg, fg, pfg, pbg, lfg;

  size = get_integer_resource ("gridSize", "Dimension");
  root = get_boolean_resource("root", "Boolean");
  solve_delay = get_integer_resource ("solveDelay", "Integer");
  pre_solve_delay = get_integer_resource ("preDelay", "Integer");
  post_solve_delay = get_integer_resource ("postDelay", "Integer");

  if (size < 2) size = 7 + (random () % 30);
  grid_width = grid_height = size;
  bw = (size > 6 ? 3 : (size-1)/2);

  dpy = display; win = window; /* the maze stuff uses global variables */

  XGetWindowAttributes (dpy, win, &xgwa);

  x = 0;
  y = 0;

  set_maze_sizes (xgwa.width, xgwa.height);

  if (! root)
    XSelectInput (dpy, win, ExposureMask|ButtonPressMask|StructureNotifyMask);
  
  gc = XCreateGC(dpy, win, 0, 0);
  cgc = XCreateGC(dpy, win, 0, 0);
  tgc = XCreateGC(dpy,win,0,0);
  logo_gc = XCreateGC(dpy, win, 0, 0);
  
  gray = XCreateBitmapFromData (dpy,win,gray1_bits,gray1_width,gray1_height);

  bg  = get_pixel_resource ("background","Background", dpy, xgwa.colormap);
  fg  = get_pixel_resource ("foreground","Foreground", dpy, xgwa.colormap);
  lfg = get_pixel_resource ("logoColor", "Foreground", dpy, xgwa.colormap);
  pfg = get_pixel_resource ("liveColor", "Foreground", dpy, xgwa.colormap);
  pbg = get_pixel_resource ("deadColor", "Foreground", dpy, xgwa.colormap);
  if (mono_p) lfg = pfg = fg;

  if (lfg == bg)
    lfg = ((bg == WhitePixel (dpy, DefaultScreen (dpy)))
	   ? BlackPixel (dpy, DefaultScreen (dpy))
	   : WhitePixel (dpy, DefaultScreen (dpy)));

  XSetForeground (dpy, gc, fg);
  XSetBackground (dpy, gc, bg);
  XSetForeground (dpy, cgc, pbg);
  XSetBackground (dpy, cgc, bg);
  XSetForeground (dpy, tgc, pfg);
  XSetBackground (dpy, tgc, bg);
  XSetForeground (dpy, logo_gc, lfg);
  XSetBackground (dpy, logo_gc, bg);

  XSetStipple (dpy, cgc, gray);
  XSetFillStyle (dpy, cgc, FillOpaqueStippled);
  
#ifdef XROGER
  {
    int w, h;
    XGCValues gcv;
    GC draw_gc, erase_gc;
    extern void skull ();
    /* round up to grid size */
    w = ((logo_width  / grid_width) + 1)  * grid_width;
    h = ((logo_height / grid_height) + 1) * grid_height;
    logo_map = XCreatePixmap (dpy, win, w, h, 1);
    gcv.foreground = 1L;
    draw_gc = XCreateGC (dpy, logo_map, GCForeground, &gcv);
    gcv.foreground = 0L;
    erase_gc= XCreateGC (dpy, logo_map, GCForeground, &gcv);
    XFillRectangle (dpy, logo_map, erase_gc, 0, 0, w, h);
    skull (dpy, logo_map, draw_gc, erase_gc, 5, 0, w-10, h-10);
    XFreeGC (dpy, draw_gc);
    XFreeGC (dpy, erase_gc);
  }
#else
  if  (!(logo_map = XCreateBitmapFromData (dpy, win, logo_bits,
					   logo_width, logo_height)))
    {
      fprintf (stderr, "Can't create logo pixmap\n");
      exit (1);
    }
#endif
  XMapRaised(dpy, win);
  srandom(getpid());

  restart = root;

  while (1) {                            /* primary execution loop [ rhess ] */
    if (check_events()) continue ;
    if (restart || stop) goto pop;
    switch (state) {
    case 1:
      initialize_maze();
      break;
    case 2:
      XClearWindow(dpy, win);
      draw_maze_border();
      break;
    case 3:
      create_maze();
      break;
    case 4:
      XSync (dpy, False);
      usleep (pre_solve_delay);
      break;
    case 5:
      solve_maze();
      break;
    case 6:
      XSync (dpy, False);
      usleep (post_solve_delay);
      state = 0 ;
      break;
    default:
      abort ();
    }
    ++state;
  pop:
    if (restart)
      {
	static XWindowAttributes wattr;
	restart = 0;
	stop = 0;
	state = 1;
	XGetWindowAttributes (dpy, win, &wattr);
	set_maze_sizes (wattr.width, wattr.height);
	XClearWindow (dpy, win);
	XSync (dpy, False);
      }
  }
}
