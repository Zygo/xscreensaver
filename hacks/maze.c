/******************************************************************************
 * [ maze ] ...
 *
 * modified:  [ 1-04-00 ]  Johannes Keukelaar <johannes@nada.kth.se>
 *              Added -ignorant option (not the default) to remove knowlege
 *              of the direction in which the exit lies.
 *
 * modified:  [ 6-28-98 ]  Zack Weinberg <zack@rabi.phys.columbia.edu>
 *
 *              Made the maze-solver somewhat more intelligent.  There are
 *              three optimizations:
 *
 *              - Straight-line lookahead: the solver does not enter dead-end
 *                corridors.  This is a win with all maze generators.
 *
 *              - First order direction choice: the solver knows where the
 *                exit is in relation to itself, and will try paths leading in
 *                that direction first. This is a major win on maze generator 1
 *                which tends to offer direct routes to the exit.
 *
 *              - Dead region elimination: the solver already has a map of
 *                all squares visited.  Whenever it starts to backtrack, it
 *                consults this map and marks off all squares that cannot be
 *                reached from the exit without crossing a square already
 *                visited.  Those squares can never contribute to the path to
 *                the exit, so it doesn't bother checking them.  This helps a
 *                lot with maze generator 2 and somewhat less with generator 1.
 *
 *              Further improvements would require knowledge of the wall map
 *              as well as the position of the exit and the squares visited.
 *              I would consider that to be cheating.  Generator 0 makes
 *              mazes which are remarkably difficult to solve mechanically --
 *              even with these optimizations the solver generally must visit
 *              at least two-thirds of the squares.  This is partially
 *              because generator 0's mazes have longer paths to the exit.
 *
 * modified:  [ 4-10-97 ]  Johannes Keukelaar <johannes@nada.kth.se>
 *              Added multiple maze creators. Robustified solver.
 *              Added bridge option.
 * modified:  [ 8-11-95 ] Ed James <james@mml.mmc.com>
 *              added fill of dead-end box to solve_maze while loop.
 * modified:  [ 3-7-93 ]  Jamie Zawinski <jwz@jwz.org>
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
#include "erase.h"

#define XSCREENSAVER_LOGO

static int solve_delay, pre_solve_delay, post_solve_delay;

#include  <stdio.h>
#include  <X11/Xlib.h>
#include  <X11/Xutil.h>

/* #include  <X11/bitmaps/gray1> */
#define gray1_width  2
#define gray1_height 2
static char gray1_bits[] = { 0x01, 0x02 };


#define MAX_MAZE_SIZE_X	1000
#define MAX_MAZE_SIZE_Y	1000

#define MOVE_LIST_SIZE  (MAX_MAZE_SIZE_X * MAX_MAZE_SIZE_Y)

#define NOT_DEAD	0x8000
#define SOLVER_VISIT    0x4000
#define START_SQUARE	0x2000
#define END_SQUARE	0x1000

#define WALL_TOP	0x8
#define WALL_RIGHT	0x4
#define WALL_BOTTOM	0x2
#define WALL_LEFT	0x1
#define WALL_ANY	0xF

#define DOOR_IN_TOP	0x800
#define DOOR_IN_RIGHT	0x400
#define DOOR_IN_BOTTOM	0x200
#define DOOR_IN_LEFT	0x100
#define DOOR_IN_ANY	0xF00

#define DOOR_OUT_TOP	0x80
#define DOOR_OUT_RIGHT	0x40
#define DOOR_OUT_BOTTOM	0x20
#define DOOR_OUT_LEFT	0x10


#define	border_x        (0)
#define	border_y        (0)

#define	get_random(x)	(random() % (x))


static int logo_x, logo_y;

#ifdef XSCREENSAVER_LOGO
# define logo_width  54
# define logo_height 54
#else
# include  <X11/bitmaps/xlogo64>
# define logo_width xlogo64_width
# define logo_height xlogo64_height
# define logo_bits xlogo64_bits
#endif

static unsigned short maze[MAX_MAZE_SIZE_X][MAX_MAZE_SIZE_Y];

static struct {
  unsigned short x;
  unsigned short y;
  unsigned char dir, ways;
} move_list[MOVE_LIST_SIZE], save_path[MOVE_LIST_SIZE], path[MOVE_LIST_SIZE];

static int maze_size_x, maze_size_y;
static int sqnum, cur_sq_x, cur_sq_y, path_length;
static int start_x, start_y, start_dir, end_x, end_y, end_dir;
static int grid_width, grid_height;
static int bw;

static Display	*dpy;
static Window	win;
static GC	gc, cgc, tgc, sgc, ugc, logo_gc, erase_gc;
static Pixmap	logo_map;

static int	x = 0, y = 0, restart = 0, stop = 1, state = 1, max_length;
static int      sync_p, bridge_p, ignorant_p;

static int
check_events (void)                        /* X event handler [ rhess ] */
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
    default:
      screenhack_handle_event(dpy, &e);
      break;
    }
    return(1);
  }
  return(0);
}


static void
set_maze_sizes (int width, int height)
{
  maze_size_x = width / grid_width;
  maze_size_y = height / grid_height;
}


static void
initialize_maze (void) /* draw the surrounding wall and start/end squares */
{
  register int i, j, wall;
  int logow = 1 + logo_width / grid_width;
  int logoh = 1 + logo_height / grid_height;
  
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
  if ((maze_size_x-logow >= 6) && (maze_size_y-logoh >= 6))
    {
      /* not closer than 3 grid units from a wall */
      logo_x = get_random (maze_size_x - logow - 5) + 3;
      logo_y = get_random (maze_size_y - logoh - 5) + 3;
      for (i=0; i<logow; i++)
	for (j=0; j<logoh; j++)
	  maze[logo_x + i][logo_y + j] |= DOOR_IN_TOP;
    }
  else
    logo_y = logo_x = -1;
}

static int choose_door (void);
static int backup (void);
static void draw_wall (int, int, int, GC);
static void draw_solid_square (int, int, int, GC);
/*static void enter_square (int);*/
static void build_wall (int, int, int);
/*static void break_wall (int, int, int);*/

static void join_sets(int, int);

/* For set_create_maze. */
/* The sets that our squares are in. */
static int *sets = 0;
/* The `list' of hedges. */
static int *hedges = 0;

#define DEBUG_SETS 0

/* Initialise the sets. */
static void 
init_sets(void)
{
  int i, t, r, x, y;

  if(sets)
    free(sets);
  sets = (int *)malloc(maze_size_x*maze_size_y*sizeof(int));
  if(!sets)
    abort();
  for(i = 0; i < maze_size_x*maze_size_y; i++)
    {
      sets[i] = i;
    }
  
  if(hedges)
    free(hedges);
  hedges = (int *)malloc(maze_size_x*maze_size_y*2*sizeof(int));
  if(!hedges)
    abort();
  for(i = 0; i < maze_size_x*maze_size_y*2; i++)
    {
      hedges[i] = i;
    }
  /* Mask out outside walls. */
  for(i = 0; i < maze_size_y; i++)
    {
      hedges[2*((maze_size_x)*i+maze_size_x-1)+1] = -1;
    }
  for(i = 0; i < maze_size_x; i++)
    {
      hedges[2*((maze_size_y-1)*maze_size_x+i)] = -1;
    }
  /* Mask out a possible logo. */
  if(logo_x!=-1)
    {
      int logow = 1 + logo_width / grid_width;
      int logoh = 1 + logo_height / grid_height;
      int bridge_dir, bridge_c;

      if(bridge_p && logoh>=3 && logow>=3)
	{
	  bridge_dir = 1+random()%2;
	  if(bridge_dir==1)
	    {
	      bridge_c = logo_y+random()%(logoh-2)+1;
	    }
	  else
	    {
	      bridge_c = logo_x+random()%(logow-2)+1;
	    }
	}
      else
	{
	  bridge_dir = 0;
	  bridge_c = -1;
	}

      for(x = logo_x; x < logo_x+logow; x++)
	for(y = logo_y; y < logo_y+logoh; y++)
	  {
	    /* I should check for the bridge here, except that I join the
             * bridge together below.
             */
	    hedges[2*(x+maze_size_x*y)+1] = -1;
	    hedges[2*(x+maze_size_x*y)] = -1;
	  }
      for(x = logo_x; x < logo_x+logow; x++)
	{
	  if(!(bridge_dir==2 && x==bridge_c))
	    {
	      build_wall(x, logo_y, 0);
	      build_wall(x, logo_y+logoh, 0);
	    }
	  hedges[2*(x+maze_size_x*(logo_y-1))] = -1;
	  if(bridge_dir==1)
	    {
	      build_wall(x, bridge_c, 0);
	      build_wall(x, bridge_c, 2);
	    }
	}
      for(y = logo_y; y < logo_y+logoh; y++)
	{
	  if(!(bridge_dir==1 && y==bridge_c))
	    {
	      build_wall(logo_x, y, 3);
	      build_wall(logo_x+logow, y, 3);
	    }
	  hedges[2*(logo_x-1+maze_size_x*y)+1] = -1;
	  if(bridge_dir==2)
	    {
	      build_wall(bridge_c, y, 1);
	      build_wall(bridge_c, y, 3);
	    }
	}
      /* Join the whole bridge together. */
      if(bridge_p)
	{
	  if(bridge_dir==1)
	    {
	      x = logo_x-1;
	      y = bridge_c;
	      for(i = logo_x; i < logo_x+logow+1; i++)
		join_sets(x+y*maze_size_x, i+y*maze_size_x);
	    }
	  else
	    {
	      y = logo_y-1;
	      x = bridge_c;
	      for(i = logo_y; i < logo_y+logoh+1; i++)
		join_sets(x+y*maze_size_x, x+i*maze_size_x);
	    }
	}
    }

  for(i = 0; i < maze_size_x*maze_size_y*2; i++)
    {
      t = hedges[i];
      r = random()%(maze_size_x*maze_size_y*2);
      hedges[i] = hedges[r];
      hedges[r] = t;
    }
}

/* Get the representative of a set. */
static int
get_set(int num)
{
  int s;

  if(sets[num]==num)
    return num;
  else
    {
      s = get_set(sets[num]);
      sets[num] = s;
      return s;
    }
}

/* Join two sets together. */
static void
join_sets(num1, num2)
     int num1, num2;
{
  int s1, s2;

  s1 = get_set(num1);
  s2 = get_set(num2);
  
  if(s1<s2)
    sets[s2] = s1;
  else
    sets[s1] = s2;
}

/* Exitialise the sets. */
static void
exit_sets(void)
{
  if(hedges)
    free(hedges);
  hedges = 0;
  if(sets)
    free(sets);
  sets = 0;
}

#if DEBUG_SETS
/* Temporary hack. */
static void
show_set(int num, GC gc)
{
  int x, y, set;

  set = get_set(num);

  for(x = 0; x < maze_size_x; x++)
    for(y = 0; y < maze_size_y; y++)
      {
	if(get_set(x+y*maze_size_x)==set)
	  {
	    XFillRectangle(dpy, win, gc,  border_x + bw + grid_width * x, 
			 border_y + bw + grid_height * y, 
			 grid_width-2*bw , grid_height-2*bw);
	  }
      }
}
#endif

/* Second alternative maze creator: Put each square in the maze in a 
 * separate set. Also, make a list of all the hedges. Randomize that list.
 * Walk through the list. If, for a certain hedge, the two squares on both
 * sides of it are in different sets, union the sets and remove the hedge.
 * Continue until all hedges have been processed or only one set remains.
 */
static void
set_create_maze(void)
{
  int i, h, x, y, dir, v, w;
#if DEBUG_SETS
  int cont = 0;
  char c;
#endif

  /* Do almost all the setup. */
  init_sets();

  /* Start running through the hedges. */
  for(i = 0; i < 2*maze_size_x*maze_size_y; i++)
    { 
      h = hedges[i];

      /* This one is in the logo or outside border. */
      if(h==-1)
	continue;

      dir = h%2?1:2;
      x = (h>>1)%maze_size_x;
      y = (h>>1)/maze_size_x;

      v = x;
      w = y;
      switch(dir)
	{
	case 1:
	  v++;
	  break;
	case 2:
	  w++;
	  break;
	}

#if DEBUG_SETS
      show_set(x+y*maze_size_x, logo_gc);
      show_set(v+w*maze_size_x, tgc);
#endif
      if(get_set(x+y*maze_size_x)!=get_set(v+w*maze_size_x))
	{
#if DEBUG_SETS
	  printf("Join!");
#endif
	  join_sets(x+y*maze_size_x, v+w*maze_size_x);
	  /* Don't draw the wall. */
	}
      else
	{
#if DEBUG_SETS
	  printf("Build.");
#endif
	  /* Don't join the sets. */
	  build_wall(x, y, dir); 
	}
#if DEBUG_SETS
      if(!cont)
	{
	  XSync(dpy, False);
	  c = getchar();
	  if(c=='c')
	    cont = 1;
	}
      show_set(x+y*maze_size_x, erase_gc);
      show_set(v+w*maze_size_x, erase_gc);
#endif
    }

  /* Free some memory. */
  exit_sets();
}

/* First alternative maze creator: Pick a random, empty corner in the maze.
 * Pick a random direction. Draw a wall in that direction, from that corner
 * until we hit a wall. Option: Only draw the wall if it's going to be 
 * shorter than a certain length. Otherwise we get lots of long walls.
 */
static void
alt_create_maze(void)
{
  char *corners;
  int *c_idx;
  int i, j, height, width, open_corners, k, dir, x, y;

  height = maze_size_y+1;
  width = maze_size_x+1;

  /* Allocate and clear some mem. */
  corners = (char *)calloc(height*width, 1);
  if(!corners)
    return;

  /* Set up the indexing array. */
  c_idx = (int *)malloc(sizeof(int)*height*width);
  if(!c_idx)
    {
      free(corners);
      return;
    }
  for(i = 0; i < height*width; i++)
    c_idx[i] = i;
  for(i = 0; i < height*width; i++)
    {
      j = c_idx[i];
      k = random()%(height*width);
      c_idx[i] = c_idx[k];
      c_idx[k] = j;
    }

  /* Set up some initial walls. */
  /* Outside walls. */
  for(i = 0; i < width; i++)
    {
      corners[i] = 1;
      corners[i+width*(height-1)] = 1;
    }
  for(i = 0; i < height; i++)
    {
      corners[i*width] = 1;
      corners[i*width+width-1] = 1;
    }
  /* Walls around logo. In fact, inside the logo, too. */
  /* Also draw the walls. */
  if(logo_x!=-1)
    {
      int logow = 1 + logo_width / grid_width;
      int logoh = 1 + logo_height / grid_height;
      int bridge_dir, bridge_c;

      if(bridge_p && logoh>=3 && logow>=3)
	{
	  bridge_dir = 1+random()%2;
	  if(bridge_dir==1)
	    {
	      bridge_c = logo_y+random()%(logoh-2)+1;
	    }
	  else
	    {
	      bridge_c = logo_x+random()%(logow-2)+1;
	    }
	}
      else
	{
	  bridge_dir = 0;
	  bridge_c = -1;
	}
      for(i = logo_x; i <= logo_x + logow; i++)
	{
	  for(j = logo_y; j <= logo_y + logoh; j++)
	    {
	      corners[i+width*j] = 1;
	    }
	}
      for(x = logo_x; x < logo_x+logow; x++)
	{
	  if(!(bridge_dir==2 && x==bridge_c))
	    {
	      build_wall(x, logo_y, 0);
	      build_wall(x, logo_y+logoh, 0);
	    }
	  if(bridge_dir==1)
	    {
	      build_wall(x, bridge_c, 0);
	      build_wall(x, bridge_c, 2);
	    }
	}
      for(y = logo_y; y < logo_y+logoh; y++)
	{
	  if(!(bridge_dir==1 && y==bridge_c))
	    {
	      build_wall(logo_x, y, 3);
	      build_wall(logo_x+logow, y, 3);
	    }
	  if(bridge_dir==2)
	    {
	      build_wall(bridge_c, y, 1);
	      build_wall(bridge_c, y, 3);
	    }
	}
      /* Connect one wall of the logo with an outside wall. */
      if(bridge_p)
	dir = (bridge_dir+1)%4;
      else
	dir = random()%4;
      switch(dir)
	{
	case 0:
	  x = logo_x+(random()%(logow+1));
	  y = logo_y;
	  break;
	case 1:
	  x = logo_x+logow;
	  y = logo_y+(random()%(logoh+1));
	  break;
	case 2:
	  x = logo_x+(random()%(logow+1));
	  y = logo_y+logoh;
	  break;
	case 3:
	  x = logo_x;
	  y = logo_y+(random()%(logoh+1));
	  break;
	}
      do
	{
	  corners[x+width*y] = 1;
	  switch(dir)
	    {
	    case 0:
	      build_wall(x-1, y-1, 1);
	      y--;
	      break;
	    case 1:
	      build_wall(x, y, 0);
	      x++;
	      break;
	    case 2:
	      build_wall(x, y, 3);
	      y++;
	      break;
	    case 3:
	      build_wall(x-1, y-1, 2);
	      x--;
	      break;	  
	    }
	}
      while(!corners[x+width*y]);
      if(bridge_p)
	{
	  dir = (dir+2)%4;
	  switch(dir)
	    {
	    case 0:
	      x = logo_x+(random()%(logow+1));
	      y = logo_y;
	      break;
	    case 1:
	      x = logo_x+logow;
	      y = logo_y+(random()%(logoh+1));
	      break;
	    case 2:
	      x = logo_x+(random()%(logow+1));
	      y = logo_y+logoh;
	      break;
	    case 3:
	      x = logo_x;
	      y = logo_y+(random()%(logoh+1));
	      break;
	    }
	  do
	    {
	      corners[x+width*y] = 1;
	      switch(dir)
		{
		case 0:
		  build_wall(x-1, y-1, 1);
		  y--;
		  break;
		case 1:
		  build_wall(x, y, 0);
		  x++;
		  break;
		case 2:
		  build_wall(x, y, 3);
		  y++;
		  break;
		case 3:
		  build_wall(x-1, y-1, 2);
		  x--;
		  break;	  
		}
	    }
	  while(!corners[x+width*y]);
	}
    }

  /* Count open gridpoints. */
  open_corners = 0;
  for(i = 0; i < width; i++)
    for(j = 0; j < height; j++)
      if(!corners[i+width*j])
	open_corners++;

  /* Now do actual maze generation. */
  while(open_corners>0)
    {
      for(i = 0; i < width*height; i++)
	{
	  if(!corners[c_idx[i]])
	    {
	      x = c_idx[i]%width;
	      y = c_idx[i]/width;
	      /* Choose a random direction. */
	      dir = random()%4;
	      
	      k = 0;
	      /* Measure the length of the wall we'd draw. */
	      while(!corners[x+width*y])
		{
		  k++;
		  switch(dir)
		    {
		    case 0:
		      y--;
		      break;
		    case 1:
		      x++;
		      break;
		    case 2:
		      y++;
		      break;
		    case 3:
		      x--;
		      break;
		    }
		}
	      
	      if(k<=max_length)
		{
		  x = c_idx[i]%width;
		  y = c_idx[i]/width;
		  
		  /* Draw a wall until we hit something. */
		  while(!corners[x+width*y])
		    {
		      open_corners--;
		      corners[x+width*y] = 1;
		      switch(dir)
			{
			case 0:
			  build_wall(x-1, y-1, 1);
			  y--;
			  break;
			case 1:
			  build_wall(x, y, 0);
			  x++;
			  break;
			case 2:
			  build_wall(x, y, 3);
			  y++;
			  break;
			case 3:
			  build_wall(x-1, y-1, 2);
			  x--;
			  break;
			}
		    }
		}
	    }
	}
    }

  /* Free some memory we used. */
  free(corners);
  free(c_idx);
}

/* The original maze creator. Start somewhere. Take a step in a random 
 * direction. Keep doing this until we hit a wall. Then, backtrack until
 * we find a point where we can go in another direction.
 */
static void
create_maze (void)    /* create a maze layout given the initialized maze */
{
  register int i, newdoor = 0;
  int logow = 1 + logo_width / grid_width;
  int logoh = 1 + logo_height / grid_height;
  
  /* Maybe we should make a bridge? */
  if(bridge_p && logo_x>=0 && logow>=3 && logoh>=3)
    {
      int bridge_dir, bridge_c;

      bridge_dir = 1+random()%2;
      if(bridge_dir==1)
	{
	  if(logoh>=3)
	    bridge_c = logo_y+random()%(logoh-2)+1;
	  else
	    bridge_c = logo_y+random()%logoh;
	}
      else
	{
	  if(logow>=3)
	    bridge_c = logo_x+random()%(logow-2)+1;
	  else
	    bridge_c = logo_x+random()%logow;
	}

      if(bridge_dir==1)
	{
	  for(i = logo_x; i < logo_x+logow; i++)
	    {
	      maze[i][bridge_c] &= ~DOOR_IN_TOP;
	    }
	}
      else
	{
	  for(i = logo_y; i < logo_y+logoh; i++)
	    {
	      maze[bridge_c][i] &= ~DOOR_IN_TOP;
	    }
	}
    }

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
choose_door (void)                                   /* pick a new path */
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
    draw_wall(cur_sq_x, cur_sq_y, 0, gc);
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
    draw_wall(cur_sq_x, cur_sq_y, 1, gc);
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
    draw_wall(cur_sq_x, cur_sq_y, 2, gc);
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
    draw_wall(cur_sq_x, cur_sq_y, 3, gc);
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
backup (void)                                          /* back up a move */
{
  sqnum--;
  cur_sq_x = move_list[sqnum].x;
  cur_sq_y = move_list[sqnum].y;
  return ( sqnum );
}


static void
draw_maze_border (void)                         /* draw the maze outline */
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

      /* round up to grid size */
      int ww = ((logo_width  / grid_width) + 1)  * grid_width;
      int hh = ((logo_height / grid_height) + 1) * grid_height;

      XGetGeometry (dpy, logo_map, &r, &x, &y, &w, &h, &bw, &d);

# ifdef XSCREENSAVER_LOGO
      /* kludge: if the logo "hole" is around the same size as the logo,
         don't center it (since the xscreensaver logo image is a little
         off center...  But do center if if the hole/gridsize is large. */
      if (ww < logo_width  + 5) ww = w; 
      if (hh < logo_height + 5) hh = h; 
# endif /* XSCREENSAVER_LOGO */

      if (d == 1)
        XCopyPlane (dpy, logo_map, win, logo_gc,
                    0, 0, w, h,
                    border_x + 3 + grid_width  * logo_x + ((ww - w) / 2),
                    border_y + 3 + grid_height * logo_y + ((hh - h) / 2),
                    1);
      else
        XCopyArea (dpy, logo_map, win, logo_gc,
                   0, 0, w, h,
                   border_x + 3 + grid_width  * logo_x + ((ww - w) / 2),
                   border_y + 3 + grid_height * logo_y + ((hh - h) / 2));
    }
  draw_solid_square (start_x, start_y, WALL_TOP >> start_dir, tgc);
  draw_solid_square (end_x, end_y, WALL_TOP >> end_dir, tgc);
}


static void
draw_wall(int i, int j, int dir, GC gc)                /* draw a single wall */
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
  if(sync_p)
    XSync(dpy, False);
}

/* Actually build a wall. */
static void
build_wall(i, j, dir)
     int i, j, dir;
{
  /* Draw it on the screen. */
  draw_wall(i, j, dir, gc);
  /* Put it in the maze. */
  switch(dir)
    {
    case 0:
      maze[i][j] |= WALL_TOP;
      if(j>0)
	maze[i][j-1] |= WALL_BOTTOM;
      break;
    case 1:
      maze[i][j] |= WALL_RIGHT;
      if(i<maze_size_x-1)
	maze[i+1][j] |= WALL_LEFT;
      break;
    case 2:
      maze[i][j] |= WALL_BOTTOM;
      if(j<maze_size_y-1)
	maze[i][j+1] |= WALL_TOP;
      break;
    case 3:
      maze[i][j] |= WALL_LEFT;
      if(i>0)
	maze[i-1][j] |= WALL_RIGHT;
      break;
    }
}

/* Break out a wall. */
#if 0
static void
break_wall(i, j, dir)
     int i, j, dir;
{
  /* Draw it on the screen. */
  draw_wall(i, j, dir, erase_gc);
  /* Put it in the maze. */
  switch(dir)
    {
    case 0:
      maze[i][j] &= ~WALL_TOP;
      if(j>0)
	maze[i][j-1] &= ~WALL_BOTTOM;
      break;
    case 1:
      maze[i][j] &= ~WALL_RIGHT;
      if(i<maze_size_x-1)
	maze[i+1][j] &= ~WALL_LEFT;
      break;
    case 2:
      maze[i][j] &= ~WALL_BOTTOM;
      if(j<maze_size_y-1)
	maze[i][j+1] &= ~WALL_BOTTOM;
      break;
    case 3:
      maze[i][j] &= ~WALL_LEFT;
      if(i>0)
	maze[i-1][j] &= ~WALL_RIGHT;
      break;
    }
}
#endif /* 0 */


static void
draw_solid_square(int i, int j,          /* draw a solid square in a square */
		  int dir, GC gc)
{
  switch (dir) {
  case WALL_TOP:
      XFillRectangle(dpy, win, gc,
		     border_x + bw+(bw==0?1:0) + grid_width * i, 
		     border_y - bw-(bw==0?1:0) + grid_height * j, 
		     grid_width - (bw+bw+(bw==0?1:0)), grid_height);
      break;
  case WALL_RIGHT:
      XFillRectangle(dpy, win, gc,
		     border_x + bw+(bw==0?1:0) + grid_width * i, 
		     border_y + bw+(bw==0?1:0) + grid_height * j, 
		     grid_width, grid_height - (bw+bw+(bw==0?1:0)));
      break;
  case WALL_BOTTOM:
      XFillRectangle(dpy, win, gc,
		     border_x + bw+(bw==0?1:0) + grid_width * i, 
		     border_y + bw+(bw==0?1:0) + grid_height * j, 
		     grid_width - (bw+bw+(bw==0?1:0)), grid_height);
      break;
  case WALL_LEFT:
      XFillRectangle(dpy, win, gc,
		     border_x - bw-(bw==0?1:0) + grid_width * i, 
		     border_y + bw+(bw==0?1:0) + grid_height * j, 
		     grid_width, grid_height - (bw+bw+(bw==0?1:0)));
      break;
  }
  XSync (dpy, False);
}

int
longdeadend_p(int x1, int y1, int x2, int y2, int endwall)
{
    int dx = x2 - x1, dy = y2 - y1;
    int sidewalls;

    sidewalls = endwall | (endwall >> 2 | endwall << 2);
    sidewalls = ~sidewalls & WALL_ANY;

    while((maze[x2][y2] & WALL_ANY) == sidewalls)
    {
	x2 += dx;
	y2 += dy;
    }

    if((maze[x2][y2] & WALL_ANY) == (sidewalls | endwall))
    {
	endwall = (endwall >> 2 | endwall << 2) & WALL_ANY;
	while(x1 != x2 || y1 != y2)
	{
	    x1 += dx;
	    y1 += dy;
	    draw_solid_square(x1, y1, endwall, sgc);
	    maze[x1][y1] |= SOLVER_VISIT;
	}
	return 1;
    }
    else
	return 0;
}

/* Find all dead regions -- areas from which the goal cannot be reached --
   and mark them visited. */
void
find_dead_regions(void)
{
    int x, y, flipped;

    /* Find all not SOLVER_VISIT squares bordering NOT_DEAD squares
       and mark them NOT_DEAD also.  Repeat until no more such squares. */
    maze[start_x][start_y] |= NOT_DEAD;
    
    do
    {
	flipped = 0;
	for(x = 0; x < maze_size_x; x++)
	    for(y = 0; y < maze_size_y; y++)
		if(!(maze[x][y] & (SOLVER_VISIT | NOT_DEAD))
		   && (   (x && (maze[x-1][y] & NOT_DEAD))
		       || (y && (maze[x][y-1] & NOT_DEAD))))
		{
		    flipped = 1;
		    maze[x][y] |= NOT_DEAD;
		}
	for(x = maze_size_x-1; x >= 0; x--)
	    for(y = maze_size_y-1; y >= 0; y--)
		if(!(maze[x][y] & (SOLVER_VISIT | NOT_DEAD))
		   && (   (x != maze_size_x-1 && (maze[x+1][y] & NOT_DEAD))
		       || (y != maze_size_y-1 && (maze[x][y+1] & NOT_DEAD))))
		{
		    flipped = 1;
		    maze[x][y] |= NOT_DEAD;
		}
    }
    while(flipped);

    for (y = 0; y < maze_size_y; y++)
      for (x = 0; x < maze_size_x; x++)
      {
	if (maze[x][y] & NOT_DEAD)
	  maze[x][y] &= ~NOT_DEAD;
	else if (!(maze[x][y] & SOLVER_VISIT))
	{
	  maze[x][y] |= SOLVER_VISIT;
	  if((x < logo_x || x > logo_x + logo_width / grid_width) ||
	     (y < logo_y || y > logo_y + logo_height / grid_height))
	  {
            /* if we are completely surrounded by walls, just draw the
               inside part */
            if ((maze[x][y] & WALL_ANY) == WALL_ANY)
	      XFillRectangle(dpy, win, ugc,
			     border_x + bw + grid_width * x,
			     border_y + bw + grid_height * y,
			     grid_width - (bw+bw), grid_height - (bw+bw));
	    else
	    {
	      if (! (maze[x][y] & WALL_LEFT))
		draw_solid_square(x, y, WALL_LEFT, ugc);
	      if (! (maze[x][y] & WALL_RIGHT))
		draw_solid_square(x, y, WALL_RIGHT, ugc);
	      if (! (maze[x][y] & WALL_TOP))
		draw_solid_square(x, y, WALL_TOP, ugc);
	      if (! (maze[x][y] & WALL_BOTTOM))
		draw_solid_square(x, y, WALL_BOTTOM, ugc);
	    }
	  }
	}
      }
    XSync(dpy, False);
}

static void
solve_maze (void)                     /* solve it with graphical feedback */
{
    int i, dir, from, x, y, ways, bt = 0;

    /* plug up the surrounding wall */
    maze[end_x][end_y] |= (WALL_TOP >> end_dir);
    
    /* initialize search path */
    i = 0;
    path[i].x = end_x;
    path[i].y = end_y;
    path[i].dir = 0;
    maze[end_x][end_y] |= SOLVER_VISIT;
    
    /* do it */
    while (1)
    {
	if ( maze[path[i].x][path[i].y] & START_SQUARE )
	    return;

	/* Abort solve on expose - cheapo repaint strategy */
	if (check_events()) return;
	
	if (solve_delay) usleep (solve_delay);
	
	if(!path[i].dir)
	{
	    ways = 0;
	    /* First visit this square.  Which adjacent squares are open? */
	    for(dir = WALL_TOP; dir & WALL_ANY; dir >>= 1)
	    {
		if(maze[path[i].x][path[i].y] & dir)
		    continue;
		
		y = path[i].y - !!(dir & WALL_TOP) + !!(dir & WALL_BOTTOM);
		x = path[i].x + !!(dir & WALL_RIGHT) - !!(dir & WALL_LEFT);
		
		if(maze[x][y] & SOLVER_VISIT)
		    continue;
		
		from = (dir << 2 & WALL_ANY) | (dir >> 2 & WALL_ANY);
		/* don't enter obvious dead ends */
		if(((maze[x][y] & WALL_ANY) | from) != WALL_ANY)
		{
		    if(!longdeadend_p(path[i].x, path[i].y, x, y, dir))
			ways |= dir;
		}
		else
		{
		    draw_solid_square(x, y, from, sgc);
		    maze[x][y] |= SOLVER_VISIT;
		}
	    }
	}
	else
	    ways = path[i].ways;
	/* ways now has a bitmask of open paths. */
	
	if(!ways)
	    goto backtrack;

	if (!ignorant_p)
	  {
	    x = path[i].x - start_x;
	    y = path[i].y - start_y;
	    /* choice one */
	    if(abs(y) <= abs(x))
	      dir = (x > 0) ? WALL_LEFT : WALL_RIGHT;
	    else
	      dir = (y > 0) ? WALL_TOP : WALL_BOTTOM;
	    
	    if(dir & ways)
	      goto found;
	    
	    /* choice two */
	    switch(dir)
	      {
	      case WALL_LEFT:
	      case WALL_RIGHT:
		dir = (y > 0) ? WALL_TOP : WALL_BOTTOM; break;
	      case WALL_TOP:
	      case WALL_BOTTOM:
		dir = (x > 0) ? WALL_LEFT : WALL_RIGHT;
	      }
	    
	    if(dir & ways)
	      goto found;
	    
	    /* choice three */
	    
	    dir = (dir << 2 & WALL_ANY) | (dir >> 2 & WALL_ANY);
	    if(dir & ways)
	      goto found;
	    
	    /* choice four */
	    dir = ways;
	    if(!dir)
	      goto backtrack;
	    
	  found: ;
	  }
	else
	  {
	    if(ways&WALL_TOP)
	      dir = WALL_TOP;
	    else if(ways&WALL_LEFT)
	      dir = WALL_LEFT;
	    else if(ways&WALL_BOTTOM)
	      dir = WALL_BOTTOM;
	    else if(ways&WALL_RIGHT)
	      dir = WALL_RIGHT;
	    else
	      goto backtrack;
	  }
	bt = 0;
	ways &= ~dir;  /* tried this one */
	
	y = path[i].y - !!(dir & WALL_TOP) + !!(dir & WALL_BOTTOM);
	x = path[i].x + !!(dir & WALL_RIGHT) - !!(dir & WALL_LEFT);
	
	/* advance in direction dir */
	path[i].dir = dir;
	path[i].ways = ways;
	draw_solid_square(path[i].x, path[i].y, dir, tgc);
	
	i++;
	path[i].dir = 0;
	path[i].ways = 0;
	path[i].x = x;
	path[i].y = y;
	maze[x][y] |= SOLVER_VISIT;
	continue;

    backtrack:
	if(i == 0)
	{
	    printf("Unsolvable maze.\n");
	    return;
	}

	if(!bt && !ignorant_p)
	    find_dead_regions();
	bt = 1;
	from = path[i-1].dir;
	from = (from << 2 & WALL_ANY) | (from >> 2 & WALL_ANY);
	
	draw_solid_square(path[i].x, path[i].y, from, cgc);
	i--;
    }
} 

#if 0
static void
enter_square (int n)                      /* move into a neighboring square */
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
#endif /* 0 */


/*
 *  jmr additions for Jamie Zawinski's <jwz@jwz.org> screensaver stuff,
 *  note that the code above this has probably been hacked about in some
 *  arbitrary way.
 */

char *progclass = "Maze";

char *defaults[] = {
  ".background:	black",
  ".foreground:	white",
  "*gridSize:	0",
  "*solveDelay:	5000",
  "*preDelay:	2000000",
  "*postDelay:	4000000",
  "*liveColor:	green",
  "*deadColor:	red",
  "*skipColor:  orange",
  "*surroundColor: slateblue",
  "*generator:  -1",
  "*maxLength:  5",
  "*syncDraw:   False",
  "*bridge:     False",
  0
};

XrmOptionDescRec options[] = {
  { "-ignorant",        ".ignorant",    XrmoptionNoArg, "True" },
  { "-no-ignorant",     ".ignorant",    XrmoptionNoArg, "False" },
  { "-grid-size",	".gridSize",	XrmoptionSepArg, 0 },
  { "-solve-delay",	".solveDelay",	XrmoptionSepArg, 0 },
  { "-pre-delay",	".preDelay",	XrmoptionSepArg, 0 },
  { "-post-delay",	".postDelay",	XrmoptionSepArg, 0 },
  { "-bg-color",	".background",	XrmoptionSepArg, 0 },
  { "-fg-color",	".foreground",	XrmoptionSepArg, 0 },
  { "-live-color",	".liveColor",	XrmoptionSepArg, 0 },
  { "-dead-color",	".deadColor",	XrmoptionSepArg, 0 },
  { "-skip-color",	".skipColor",	XrmoptionSepArg, 0 },
  { "-surround-color",	".surroundColor",XrmoptionSepArg, 0 },
  { "-generator",       ".generator",   XrmoptionSepArg, 0 },
  { "-max-length",      ".maxLength",   XrmoptionSepArg, 0 },
  { "-bridge",          ".bridge",      XrmoptionNoArg, "True" },
  { "-no-bridge",       ".bridge",      XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};

void
screenhack(Display *display, Window window)
{
  Pixmap gray;
  int size, root, generator, this_gen;
  XWindowAttributes xgwa;
  unsigned long bg, fg, pfg, pbg, sfg, ufg;

  size = get_integer_resource ("gridSize", "Dimension");
  root = get_boolean_resource("root", "Boolean");
  solve_delay = get_integer_resource ("solveDelay", "Integer");
  pre_solve_delay = get_integer_resource ("preDelay", "Integer");
  post_solve_delay = get_integer_resource ("postDelay", "Integer");
  generator = get_integer_resource("generator", "Integer");
  max_length = get_integer_resource("maxLength", "Integer");
  bridge_p = get_boolean_resource("bridge", "Boolean");
  ignorant_p = get_boolean_resource("ignorant", "Boolean");

  if (size < 2) size = 7 + (random () % 30);
  grid_width = grid_height = size;
  bw = (size > 6 ? 3 : (size-1)/2);

  dpy = display; win = window; /* the maze stuff uses global variables */

  XGetWindowAttributes (dpy, win, &xgwa);

  x = 0;
  y = 0;

  set_maze_sizes (xgwa.width, xgwa.height);

  if (! root)
    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (dpy, window, &xgwa);
      XSelectInput (dpy, win,
                    xgwa.your_event_mask | ExposureMask | ButtonPressMask);
    }
  
  gc  = XCreateGC(dpy, win, 0, 0);
  cgc = XCreateGC(dpy, win, 0, 0);
  tgc = XCreateGC(dpy, win, 0, 0);
  sgc = XCreateGC(dpy, win, 0, 0);
  ugc = XCreateGC(dpy, win, 0, 0);
  logo_gc = XCreateGC(dpy, win, 0, 0);
  erase_gc = XCreateGC(dpy, win, 0, 0);
  
  gray = XCreateBitmapFromData (dpy,win,gray1_bits,gray1_width,gray1_height);

  bg  = get_pixel_resource ("background","Background", dpy, xgwa.colormap);
  fg  = get_pixel_resource ("foreground","Foreground", dpy, xgwa.colormap);
  pfg = get_pixel_resource ("liveColor", "Foreground", dpy, xgwa.colormap);
  pbg = get_pixel_resource ("deadColor", "Foreground", dpy, xgwa.colormap);
  sfg = get_pixel_resource ("skipColor", "Foreground", dpy, xgwa.colormap);
  ufg = get_pixel_resource ("surroundColor", "Foreground", dpy, xgwa.colormap);

  XSetForeground (dpy, gc, fg);
  XSetBackground (dpy, gc, bg);
  XSetForeground (dpy, cgc, pbg);
  XSetBackground (dpy, cgc, bg);
  XSetForeground (dpy, tgc, pfg);
  XSetBackground (dpy, tgc, bg);
  XSetForeground (dpy, sgc, sfg);
  XSetBackground (dpy, sgc, bg);
  XSetForeground (dpy, ugc, ufg);
  XSetBackground (dpy, ugc, bg);
  XSetForeground (dpy, logo_gc, fg);
  XSetBackground (dpy, logo_gc, bg);
  XSetForeground (dpy, erase_gc, bg);
  XSetBackground (dpy, erase_gc, bg);

  XSetStipple (dpy, cgc, gray);
  XSetFillStyle (dpy, cgc, FillOpaqueStippled);
  XSetStipple (dpy, sgc, gray);
  XSetFillStyle (dpy, sgc, FillOpaqueStippled);
  XSetStipple (dpy, ugc, gray);
  XSetFillStyle (dpy, ugc, FillOpaqueStippled);
  
#ifdef XSCREENSAVER_LOGO
  {
    unsigned long *pixels; /* ignored - unfreed */
    int npixels;
    logo_map = xscreensaver_logo (xgwa.screen, xgwa.visual, win,
                                  xgwa.colormap, bg,
                                  &pixels, &npixels, 0,
                                  logo_width > 150);
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

  restart = root;

  sync_p = !(random() % 10);

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
      this_gen = generator;
      if(this_gen<0 || this_gen>2)
	this_gen = random()%3;

      switch(this_gen)
	{
	case 0:
	  create_maze();
	  break;
	case 1:
	  alt_create_maze();
	  break;
	case 2:
	  set_create_maze();
	  break;
	}
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
      erase_full_window(display, window);
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
	sync_p = !(random() % 10);
      }
  }
}
