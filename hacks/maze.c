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

#include  <stdio.h>

/* #include  <X11/bitmaps/gray1> */
#define gray1_width  2
#define gray1_height 2
static const char gray1_bits[] = { 0x01, 0x02 };


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


struct move_list_struct {
  unsigned short x;
  unsigned short y;
  unsigned char dir, ways;
};

struct solve_state {
  int running;
  int i, x, y, bt;
};


struct state {
  Display *dpy;
  Window window;

  GC	gc, cgc, tgc, sgc, ugc, logo_gc, erase_gc;
  Pixmap logo_map;
  int logo_width, logo_height; /* in pixels */

  int solve_delay, pre_solve_delay, post_solve_delay;
  int logo_x, logo_y;

  unsigned short maze[MAX_MAZE_SIZE_X][MAX_MAZE_SIZE_Y];

  struct move_list_struct move_list[MOVE_LIST_SIZE];
  struct move_list_struct save_path[MOVE_LIST_SIZE];
  struct move_list_struct path[MOVE_LIST_SIZE];

  int maze_size_x, maze_size_y;
  int sqnum, cur_sq_x, cur_sq_y, path_length;
  int start_x, start_y, start_dir, end_x, end_y, end_dir;
  int grid_width, grid_height;
  int bw;

  int x, y, restart, stop, state, max_length;
  int sync_p, sync_tick;
  int bridge_p, ignorant_p;

  struct solve_state *solve_state;

  int *sets;		/* The sets that our squares are in. */
  int *hedges;		/* The `list' of hedges. */

  int this_gen;
  int erase_window;
  eraser_state *eraser;

  int ifrandom;
  int ifinit;

};


static void
set_maze_sizes (struct state *st, int width, int height)
{
  st->maze_size_x = width / st->grid_width;
  st->maze_size_y = height / st->grid_height;

  if (st->maze_size_x < 4) st->maze_size_x = 4;
  if (st->maze_size_y < 4) st->maze_size_y = 4;
}


static void
initialize_maze (struct state *st) /* draw the surrounding wall and start/end squares */
{
  register int i, j, wall;
  int logow = 1 + st->logo_width / st->grid_width;
  int logoh = 1 + st->logo_height / st->grid_height;
  
  /* initialize all squares */
  for ( i=0; i<st->maze_size_x; i++) {
    for ( j=0; j<st->maze_size_y; j++) {
      st->maze[i][j] = 0;
    }
  }
  
  /* top wall */
  for ( i=0; i<st->maze_size_x; i++ ) {
    st->maze[i][0] |= WALL_TOP;
  }
  
  /* right wall */
  for ( j=0; j<st->maze_size_y; j++ ) {
    st->maze[st->maze_size_x-1][j] |= WALL_RIGHT;
  }
  
  /* bottom wall */
  for ( i=0; i<st->maze_size_x; i++ ) {
    st->maze[i][st->maze_size_y-1] |= WALL_BOTTOM;
  }
  
  /* left wall */
  for ( j=0; j<st->maze_size_y; j++ ) {
    st->maze[0][j] |= WALL_LEFT;
  }
  
  /* set start square */
  wall = get_random(4);
  switch (wall) {
  case 0:	
    i = get_random(st->maze_size_x);
    j = 0;
    break;
  case 1:	
    i = st->maze_size_x - 1;
    j = get_random(st->maze_size_y);
    break;
  case 2:	
    i = get_random(st->maze_size_x);
    j = st->maze_size_y - 1;
    break;
  case 3:	
    i = 0;
    j = get_random(st->maze_size_y);
    break;
  }
  st->maze[i][j] |= START_SQUARE;
  st->maze[i][j] |= ( DOOR_IN_TOP >> wall );
  st->maze[i][j] &= ~( WALL_TOP >> wall );
  st->cur_sq_x = i;
  st->cur_sq_y = j;
  st->start_x = i;
  st->start_y = j;
  st->start_dir = wall;
  st->sqnum = 0;
  
  /* set end square */
  wall = (wall + 2)%4;
  switch (wall) {
  case 0:
    i = get_random(st->maze_size_x);
    j = 0;
    break;
  case 1:
    i = st->maze_size_x - 1;
    j = get_random(st->maze_size_y);
    break;
  case 2:
    i = get_random(st->maze_size_x);
    j = st->maze_size_y - 1;
    break;
  case 3:
    i = 0;
    j = get_random(st->maze_size_y);
    break;
  }
  st->maze[i][j] |= END_SQUARE;
  st->maze[i][j] |= ( DOOR_OUT_TOP >> wall );
  st->maze[i][j] &= ~( WALL_TOP >> wall );
  st->end_x = i;
  st->end_y = j;
  st->end_dir = wall;
  
  /* set logo */
  if ((st->maze_size_x-logow >= 6) && (st->maze_size_y-logoh >= 6))
    {
      /* not closer than 3 grid units from a wall */
      st->logo_x = get_random (st->maze_size_x - logow - 5) + 3;
      st->logo_y = get_random (st->maze_size_y - logoh - 5) + 3;
      for (i=0; i<logow; i++)
	for (j=0; j<logoh; j++)
	  st->maze[st->logo_x + i][st->logo_y + j] |= DOOR_IN_TOP;
    }
  else
    st->logo_y = st->logo_x = -1;
}

static int choose_door (struct state *st);
static int backup (struct state *st);
static void draw_wall (struct state *, int, int, int, GC);
static void draw_solid_square (struct state *, int, int, int, GC);
/*static void enter_square (struct state *, int);*/
static void build_wall (struct state *, int, int, int);
/*static void break_wall (struct state *, int, int, int);*/

static void join_sets(struct state *, int, int);

#define DEBUG_SETS 0

/* Initialise the sets. */
static void 
init_sets(struct state *st)
{
  int i, t, r, xx, yy;

  if(st->sets)
    free(st->sets);
  st->sets = (int *)malloc(st->maze_size_x*st->maze_size_y*sizeof(int));
  if(!st->sets)
    abort();
  for(i = 0; i < st->maze_size_x*st->maze_size_y; i++)
    {
      st->sets[i] = i;
    }
  
  if(st->hedges)
    free(st->hedges);
  st->hedges = (int *)malloc(st->maze_size_x*st->maze_size_y*2*sizeof(int));
  if(!st->hedges)
    abort();
  for(i = 0; i < st->maze_size_x*st->maze_size_y*2; i++)
    {
      st->hedges[i] = i;
    }
  /* Mask out outside walls. */
  for(i = 0; i < st->maze_size_y; i++)
    {
      st->hedges[2*((st->maze_size_x)*i+st->maze_size_x-1)+1] = -1;
    }
  for(i = 0; i < st->maze_size_x; i++)
    {
      st->hedges[2*((st->maze_size_y-1)*st->maze_size_x+i)] = -1;
    }
  /* Mask out a possible logo. */
  if(st->logo_x!=-1)
    {
      int logow = 1 + st->logo_width / st->grid_width;
      int logoh = 1 + st->logo_height / st->grid_height;
      int bridge_dir, bridge_c;

      if(st->bridge_p && logoh>=3 && logow>=3)
	{
	  bridge_dir = 1+random()%2;
	  if(bridge_dir==1)
	    {
	      bridge_c = st->logo_y+random()%(logoh-2)+1;
	    }
	  else
	    {
	      bridge_c = st->logo_x+random()%(logow-2)+1;
	    }
	}
      else
	{
	  bridge_dir = 0;
	  bridge_c = -1;
	}

      for(xx = st->logo_x; xx < st->logo_x+logow; xx++)
	for(yy = st->logo_y; yy < st->logo_y+logoh; yy++)
	  {
	    /* I should check for the bridge here, except that I join the
             * bridge together below.
             */
	    st->hedges[2*(xx+st->maze_size_x*yy)+1] = -1;
	    st->hedges[2*(xx+st->maze_size_x*yy)] = -1;
	  }
      for(xx = st->logo_x; xx < st->logo_x+logow; xx++)
	{
	  if(!(bridge_dir==2 && xx==bridge_c))
	    {
	      build_wall(st, xx, st->logo_y, 0);
	      build_wall(st, xx, st->logo_y+logoh, 0);
	    }
	  st->hedges[2*(xx+st->maze_size_x*(st->logo_y-1))] = -1;
	  if(bridge_dir==1)
	    {
	      build_wall(st, xx, bridge_c, 0);
	      build_wall(st, xx, bridge_c, 2);
	    }
	}
      for(yy = st->logo_y; yy < st->logo_y+logoh; yy++)
	{
	  if(!(bridge_dir==1 && yy==bridge_c))
	    {
	      build_wall(st, st->logo_x, yy, 3);
	      build_wall(st, st->logo_x+logow, yy, 3);
	    }
	  st->hedges[2*(st->logo_x-1+st->maze_size_x*yy)+1] = -1;
	  if(bridge_dir==2)
	    {
	      build_wall(st, bridge_c, yy, 1);
	      build_wall(st, bridge_c, yy, 3);
	    }
	}
      /* Join the whole bridge together. */
      if(st->bridge_p)
	{
	  if(bridge_dir==1)
	    {
	      xx = st->logo_x-1;
	      yy = bridge_c;
	      for(i = st->logo_x; i < st->logo_x+logow+1; i++)
		join_sets(st, xx+yy*st->maze_size_x, i+yy*st->maze_size_x);
	    }
	  else
	    {
	      yy = st->logo_y-1;
	      xx = bridge_c;
	      for(i = st->logo_y; i < st->logo_y+logoh+1; i++)
		join_sets(st, xx+yy*st->maze_size_x, xx+i*st->maze_size_x);
	    }
	}
    }

  for(i = 0; i < st->maze_size_x*st->maze_size_y*2; i++)
    {
      t = st->hedges[i];
      r = random()%(st->maze_size_x*st->maze_size_y*2);
      st->hedges[i] = st->hedges[r];
      st->hedges[r] = t;
    }
}

/* Get the representative of a set. */
static int
get_set(struct state *st, int num)
{
  int s;

  if(st->sets[num]==num)
    return num;
  else
    {
      s = get_set(st, st->sets[num]);
      st->sets[num] = s;
      return s;
    }
}

/* Join two sets together. */
static void
join_sets(struct state *st, int num1, int num2)
{
  int s1, s2;

  s1 = get_set(st, num1);
  s2 = get_set(st, num2);
  
  if(s1<s2)
    st->sets[s2] = s1;
  else
    st->sets[s1] = s2;
}

/* Exitialise the sets. */
static void
exit_sets(struct state *st)
{
  if(st->hedges)
    free(st->hedges);
  st->hedges = 0;
  if(st->sets)
    free(st->sets);
  st->sets = 0;
}

#if DEBUG_SETS
/* Temporary hack. */
static void
show_set(int num, GC gc)
{
  int st->x, st->y, set;

  set = get_set(num);

  for(st->x = 0; st->x < st->maze_size_x; st->x++)
    for(st->y = 0; st->y < st->maze_size_y; st->y++)
      {
	if(get_set(st->x+st->y*st->maze_size_x)==set)
	  {
	    XFillRectangle(st->dpy, st->window, st->gc,  border_x + st->bw + st->grid_width * st->x, 
			 border_y + st->bw + st->grid_height * st->y, 
			 st->grid_width-2*st->bw , st->grid_height-2*st->bw);
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
set_create_maze(struct state *st)
{
  int i, h, xx, yy, dir, v, w;
#if DEBUG_SETS
  int cont = 0;
  char c;
#endif

  /* Do almost all the setup. */
  init_sets(st);

  /* Start running through the hedges. */
  for(i = 0; i < 2*st->maze_size_x*st->maze_size_y; i++)
    { 
      h = st->hedges[i];

      /* This one is in the logo or outside border. */
      if(h==-1)
	continue;

      dir = h%2?1:2;
      xx = (h>>1)%st->maze_size_x;
      yy = (h>>1)/st->maze_size_x;

      v = xx;
      w = yy;
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
      show_set(st, xx+yy*st->maze_size_x, st->logo_gc);
      show_set(st, v+w*st->maze_size_x, st->tgc);
#endif
      if(get_set(st, xx+yy*st->maze_size_x)!=get_set(st, v+w*st->maze_size_x))
	{
#if DEBUG_SETS
	  printf("Join!");
#endif
	  join_sets(st, xx+yy*st->maze_size_x, v+w*st->maze_size_x);
	  /* Don't draw the wall. */
	}
      else
	{
#if DEBUG_SETS
	  printf("Build.");
#endif
	  /* Don't join the sets. */
	  build_wall(st, xx, yy, dir); 
	}
#if DEBUG_SETS
      if(!cont)
	{
	  XSync(st->dpy, False);
	  c = getchar();
	  if(c=='c')
	    cont = 1;
	}
      show_set(xx+yy*st->maze_size_x, st->erase_gc);
      show_set(v+w*st->maze_size_x, st->erase_gc);
#endif
    }

  /* Free some memory. */
  exit_sets(st);
}

/* First alternative maze creator: Pick a random, empty corner in the maze.
 * Pick a random direction. Draw a wall in that direction, from that corner
 * until we hit a wall. Option: Only draw the wall if it's going to be 
 * shorter than a certain length. Otherwise we get lots of long walls.
 */
static void
alt_create_maze(struct state *st)
{
  char *corners;
  int *c_idx;
  int i, j, height, width, open_corners, k, dir, xx, yy;

  height = st->maze_size_y+1;
  width = st->maze_size_x+1;

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
  if(st->logo_x!=-1)
    {
      int logow = 1 + st->logo_width / st->grid_width;
      int logoh = 1 + st->logo_height / st->grid_height;
      int bridge_dir, bridge_c;

      if(st->bridge_p && logoh>=3 && logow>=3)
	{
	  bridge_dir = 1+random()%2;
	  if(bridge_dir==1)
	    {
	      bridge_c = st->logo_y+random()%(logoh-2)+1;
	    }
	  else
	    {
	      bridge_c = st->logo_x+random()%(logow-2)+1;
	    }
	}
      else
	{
	  bridge_dir = 0;
	  bridge_c = -1;
	}
      for(i = st->logo_x; i <= st->logo_x + logow; i++)
	{
	  for(j = st->logo_y; j <= st->logo_y + logoh; j++)
	    {
	      corners[i+width*j] = 1;
	    }
	}
      for(xx = st->logo_x; xx < st->logo_x+logow; xx++)
	{
	  if(!(bridge_dir==2 && xx==bridge_c))
	    {
	      build_wall(st, xx, st->logo_y, 0);
	      build_wall(st, xx, st->logo_y+logoh, 0);
	    }
	  if(bridge_dir==1)
	    {
	      build_wall(st, xx, bridge_c, 0);
	      build_wall(st, xx, bridge_c, 2);
	    }
	}
      for(yy = st->logo_y; yy < st->logo_y+logoh; yy++)
	{
	  if(!(bridge_dir==1 && yy==bridge_c))
	    {
	      build_wall(st, st->logo_x, yy, 3);
	      build_wall(st, st->logo_x+logow, yy, 3);
	    }
	  if(bridge_dir==2)
	    {
	      build_wall(st, bridge_c, yy, 1);
	      build_wall(st, bridge_c, yy, 3);
	    }
	}
      /* Connect one wall of the logo with an outside wall. */
      if(st->bridge_p)
	dir = (bridge_dir+1)%4;
      else
	dir = random()%4;
      switch(dir)
	{
	case 0:
	  xx = st->logo_x+(random()%(logow+1));
	  yy = st->logo_y;
	  break;
	case 1:
	  xx = st->logo_x+logow;
	  yy = st->logo_y+(random()%(logoh+1));
	  break;
	case 2:
	  xx = st->logo_x+(random()%(logow+1));
	  yy = st->logo_y+logoh;
	  break;
	case 3:
	  xx = st->logo_x;
	  yy = st->logo_y+(random()%(logoh+1));
	  break;
	}
      do
	{
	  corners[xx+width*yy] = 1;
	  switch(dir)
	    {
	    case 0:
	      build_wall(st, xx-1, yy-1, 1);
	      yy--;
	      break;
	    case 1:
	      build_wall(st, xx, yy, 0);
	      xx++;
	      break;
	    case 2:
	      build_wall(st, xx, yy, 3);
	      yy++;
	      break;
	    case 3:
	      build_wall(st, xx-1, yy-1, 2);
	      xx--;
	      break;	  
	    }
	}
      while(!corners[xx+width*yy]);
      if(st->bridge_p)
	{
	  dir = (dir+2)%4;
	  switch(dir)
	    {
	    case 0:
	      xx = st->logo_x+(random()%(logow+1));
	      yy = st->logo_y;
	      break;
	    case 1:
	      xx = st->logo_x+logow;
	      yy = st->logo_y+(random()%(logoh+1));
	      break;
	    case 2:
	      xx = st->logo_x+(random()%(logow+1));
	      yy = st->logo_y+logoh;
	      break;
	    case 3:
	      xx = st->logo_x;
	      yy = st->logo_y+(random()%(logoh+1));
	      break;
	    }
	  do
	    {
	      corners[xx+width*yy] = 1;
	      switch(dir)
		{
		case 0:
		  build_wall(st, xx-1, yy-1, 1);
		  yy--;
		  break;
		case 1:
		  build_wall(st, xx, yy, 0);
		  xx++;
		  break;
		case 2:
		  build_wall(st, xx, yy, 3);
		  yy++;
		  break;
		case 3:
		  build_wall(st, xx-1, yy-1, 2);
		  xx--;
		  break;	  
		}
	    }
	  while(!corners[xx+width*yy]);
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
	      xx = c_idx[i]%width;
	      yy = c_idx[i]/width;
	      /* Choose a random direction. */
	      dir = random()%4;
	      
	      k = 0;
	      /* Measure the length of the wall we'd draw. */
	      while(!corners[xx+width*yy])
		{
		  k++;
		  switch(dir)
		    {
		    case 0:
		      yy--;
		      break;
		    case 1:
		      xx++;
		      break;
		    case 2:
		      yy++;
		      break;
		    case 3:
		      xx--;
		      break;
		    }
		}
	      
	      if(k<=st->max_length)
		{
		  xx = c_idx[i]%width;
		  yy = c_idx[i]/width;
		  
		  /* Draw a wall until we hit something. */
		  while(!corners[xx+width*yy])
		    {
		      open_corners--;
		      corners[xx+width*yy] = 1;
		      switch(dir)
			{
			case 0:
			  build_wall(st, xx-1, yy-1, 1);
			  yy--;
			  break;
			case 1:
			  build_wall(st, xx, yy, 0);
			  xx++;
			  break;
			case 2:
			  build_wall(st, xx, yy, 3);
			  yy++;
			  break;
			case 3:
			  build_wall(st, xx-1, yy-1, 2);
			  xx--;
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
create_maze (struct state *st)    /* create a maze layout given the initialized maze */
{
  register int i, newdoor = 0;
  int logow = 1 + st->logo_width / st->grid_width;
  int logoh = 1 + st->logo_height / st->grid_height;
  
  /* Maybe we should make a bridge? */
  if(st->bridge_p && st->logo_x >= 0 && logow>=3 && logoh>=3)
    {
      int bridge_dir, bridge_c;

      bridge_dir = 1+random()%2;
      if(bridge_dir==1)
	{
	  if(logoh>=3)
	    bridge_c = st->logo_y+random()%(logoh-2)+1;
	  else
	    bridge_c = st->logo_y+random()%logoh;
	}
      else
	{
	  if(logow>=3)
	    bridge_c = st->logo_x+random()%(logow-2)+1;
	  else
	    bridge_c = st->logo_x+random()%logow;
	}

      if(bridge_dir==1)
	{
	  for(i = st->logo_x; i < st->logo_x+logow; i++)
	    {
	      st->maze[i][bridge_c] &= ~DOOR_IN_TOP;
	    }
	}
      else
	{
	  for(i = st->logo_y; i < st->logo_y+logoh; i++)
	    {
	      st->maze[bridge_c][i] &= ~DOOR_IN_TOP;
	    }
	}
    }

  do {
    st->move_list[st->sqnum].x = st->cur_sq_x;
    st->move_list[st->sqnum].y = st->cur_sq_y;
    st->move_list[st->sqnum].dir = newdoor;
    while ( ( newdoor = choose_door(st) ) == -1 ) { /* pick a door */
      if ( backup(st) == -1 ) { /* no more doors ... backup */
	return; /* done ... return */
      }
    }
    
    /* mark the out door */
    st->maze[st->cur_sq_x][st->cur_sq_y] |= ( DOOR_OUT_TOP >> newdoor );
    
    switch (newdoor) {
    case 0: st->cur_sq_y--;
      break;
    case 1: st->cur_sq_x++;
      break;
    case 2: st->cur_sq_y++;
      break;
    case 3: st->cur_sq_x--;
      break;
    }
    st->sqnum++;
    
    /* mark the in door */
    st->maze[st->cur_sq_x][st->cur_sq_y] |= ( DOOR_IN_TOP >> ((newdoor+2)%4) );
    
    /* if end square set path length and save path */
    if ( st->maze[st->cur_sq_x][st->cur_sq_y] & END_SQUARE ) {
      st->path_length = st->sqnum;
      for ( i=0; i<st->path_length; i++) {
	st->save_path[i].x = st->move_list[i].x;
	st->save_path[i].y = st->move_list[i].y;
	st->save_path[i].dir = st->move_list[i].dir;
      }
    }
    
  } while (1);
  
}


static int
choose_door (struct state *st)                                   /* pick a new path */
{
  int candidates[3];
  register int num_candidates;
  
  num_candidates = 0;
  
  /* top wall */
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & DOOR_IN_TOP )
    goto rightwall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & DOOR_OUT_TOP )
    goto rightwall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & WALL_TOP )
    goto rightwall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y - 1] & DOOR_IN_ANY ) {
    st->maze[st->cur_sq_x][st->cur_sq_y] |= WALL_TOP;
    st->maze[st->cur_sq_x][st->cur_sq_y - 1] |= WALL_BOTTOM;
    draw_wall(st, st->cur_sq_x, st->cur_sq_y, 0, st->gc);
    goto rightwall;
  }
  candidates[num_candidates++] = 0;
  
 rightwall:
  /* right wall */
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & DOOR_IN_RIGHT )
    goto bottomwall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & DOOR_OUT_RIGHT )
    goto bottomwall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & WALL_RIGHT )
    goto bottomwall;
  if ( st->maze[st->cur_sq_x + 1][st->cur_sq_y] & DOOR_IN_ANY ) {
    st->maze[st->cur_sq_x][st->cur_sq_y] |= WALL_RIGHT;
    st->maze[st->cur_sq_x + 1][st->cur_sq_y] |= WALL_LEFT;
    draw_wall(st, st->cur_sq_x, st->cur_sq_y, 1, st->gc);
    goto bottomwall;
  }
  candidates[num_candidates++] = 1;
  
 bottomwall:
  /* bottom wall */
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & DOOR_IN_BOTTOM )
    goto leftwall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & DOOR_OUT_BOTTOM )
    goto leftwall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & WALL_BOTTOM )
    goto leftwall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y + 1] & DOOR_IN_ANY ) {
    st->maze[st->cur_sq_x][st->cur_sq_y] |= WALL_BOTTOM;
    st->maze[st->cur_sq_x][st->cur_sq_y + 1] |= WALL_TOP;
    draw_wall(st, st->cur_sq_x, st->cur_sq_y, 2, st->gc);
    goto leftwall;
  }
  candidates[num_candidates++] = 2;
  
 leftwall:
  /* left wall */
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & DOOR_IN_LEFT )
    goto donewall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & DOOR_OUT_LEFT )
    goto donewall;
  if ( st->maze[st->cur_sq_x][st->cur_sq_y] & WALL_LEFT )
    goto donewall;
  if ( st->maze[st->cur_sq_x - 1][st->cur_sq_y] & DOOR_IN_ANY ) {
    st->maze[st->cur_sq_x][st->cur_sq_y] |= WALL_LEFT;
    st->maze[st->cur_sq_x - 1][st->cur_sq_y] |= WALL_RIGHT;
    draw_wall(st, st->cur_sq_x, st->cur_sq_y, 3, st->gc);
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
backup (struct state *st)                                          /* back up a move */
{
  st->sqnum--;
  st->cur_sq_x = st->move_list[st->sqnum].x;
  st->cur_sq_y = st->move_list[st->sqnum].y;
  return ( st->sqnum );
}


static void
draw_maze_border (struct state *st)                         /* draw the maze outline */
{
  register int i, j;
  
  
  for ( i=0; i<st->maze_size_x; i++) {
    if ( st->maze[i][0] & WALL_TOP ) {
      XDrawLine(st->dpy, st->window, st->gc,
		border_x + st->grid_width * i,
		border_y,
		border_x + st->grid_width * (i+1) - 1,
		border_y);
    }
    if ((st->maze[i][st->maze_size_y - 1] & WALL_BOTTOM)) {
      XDrawLine(st->dpy, st->window, st->gc,
		border_x + st->grid_width * i,
		border_y + st->grid_height * (st->maze_size_y) - 1,
		border_x + st->grid_width * (i+1) - 1,
		border_y + st->grid_height * (st->maze_size_y) - 1);
    }
  }
  for ( j=0; j<st->maze_size_y; j++) {
    if ( st->maze[st->maze_size_x - 1][j] & WALL_RIGHT ) {
      XDrawLine(st->dpy, st->window, st->gc,
		border_x + st->grid_width * st->maze_size_x - 1,
		border_y + st->grid_height * j,
		border_x + st->grid_width * st->maze_size_x - 1,
		border_y + st->grid_height * (j+1) - 1);
    }
    if ( st->maze[0][j] & WALL_LEFT ) {
      XDrawLine(st->dpy, st->window, st->gc,
		border_x,
		border_y + st->grid_height * j,
		border_x,
		border_y + st->grid_height * (j+1) - 1);
    }
  }
  
  if (st->logo_x != -1)
    {
      Window r;
      int xx, yy;
      unsigned int w, h, bbw, d;

      /* round up to grid size */
      int ww = ((st->logo_width  / st->grid_width) + 1)  * st->grid_width;
      int hh = ((st->logo_height / st->grid_height) + 1) * st->grid_height;
      int lx, ly;

      XGetGeometry (st->dpy, st->logo_map, &r, &xx, &yy, &w, &h, &bbw, &d);

      /* kludge: if the logo "hole" is around the same size as the logo,
         don't center it (since the xscreensaver logo image is a little
         off center...  But do center it if the hole/gridsize is large. */
      if (ww < st->logo_width  + 5) ww = w; 
      if (hh < st->logo_height + 5) hh = h; 


      lx = border_x + 3 + st->grid_width  * st->logo_x + ((ww - w) / 2);
      ly = border_y + 3 + st->grid_height * st->logo_y + ((hh - h) / 2);

      /* Fill the background of the logo box with the "unreachable" color */
      XFillRectangle (st->dpy, st->window, st->ugc, 
                      border_x + 3 + st->grid_width  * st->logo_x,
                      border_y + 3 + st->grid_height * st->logo_y,
                      ww, hh);

      XSetClipOrigin (st->dpy, st->logo_gc, lx, ly);
      if (d == 1)
        XCopyPlane (st->dpy, st->logo_map, st->window, st->logo_gc,
                    0, 0, w, h, lx, ly, 1);
      else
        XCopyArea (st->dpy, st->logo_map, st->window, st->logo_gc,
                   0, 0, w, h, lx, ly);
    }
  draw_solid_square (st, st->start_x, st->start_y, WALL_TOP >> st->start_dir, st->tgc);
  draw_solid_square (st, st->end_x, st->end_y, WALL_TOP >> st->end_dir, st->tgc);
}


static void
draw_wall(struct state *st, int i, int j, int dir, GC with_gc)                /* draw a single wall */
{
  switch (dir) {
  case 0:
    XDrawLine(st->dpy, st->window, with_gc,
	      border_x + st->grid_width * i, 
	      border_y + st->grid_height * j,
	      border_x + st->grid_width * (i+1), 
	      border_y + st->grid_height * j);
    break;
  case 1:
    XDrawLine(st->dpy, st->window, with_gc,
	      border_x + st->grid_width * (i+1), 
	      border_y + st->grid_height * j,
	      border_x + st->grid_width * (i+1), 
	      border_y + st->grid_height * (j+1));
    break;
  case 2:
    XDrawLine(st->dpy, st->window, with_gc,
	      border_x + st->grid_width * i, 
	      border_y + st->grid_height * (j+1),
	      border_x + st->grid_width * (i+1), 
	      border_y + st->grid_height * (j+1));
    break;
  case 3:
    XDrawLine(st->dpy, st->window, with_gc,
	      border_x + st->grid_width * i, 
	      border_y + st->grid_height * j,
	      border_x + st->grid_width * i, 
	      border_y + st->grid_height * (j+1));
    break;
  }

  if(st->sync_p) {
    /* too slow if we sync on every wall, so only sync about ten times
       during the maze-creation process. 
     */
    st->sync_tick--;
    if (st->sync_tick <= 0) {
      XSync(st->dpy, False);
      st->sync_tick = st->maze_size_x * st->maze_size_x / 10;
    }
  }
}

/* Actually build a wall. */
static void
build_wall(struct state *st, int i, int j, int dir)
{
  /* Draw it on the screen. */
  draw_wall(st, i, j, dir, st->gc);
  /* Put it in the maze. */
  switch(dir)
    {
    case 0:
      st->maze[i][j] |= WALL_TOP;
      if(j>0)
	st->maze[i][j-1] |= WALL_BOTTOM;
      break;
    case 1:
      st->maze[i][j] |= WALL_RIGHT;
      if(i<st->maze_size_x-1)
	st->maze[i+1][j] |= WALL_LEFT;
      break;
    case 2:
      st->maze[i][j] |= WALL_BOTTOM;
      if(j<st->maze_size_y-1)
	st->maze[i][j+1] |= WALL_TOP;
      break;
    case 3:
      st->maze[i][j] |= WALL_LEFT;
      if(i>0)
	st->maze[i-1][j] |= WALL_RIGHT;
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
  draw_wall(i, j, dir, st->erase_gc);
  /* Put it in the maze. */
  switch(dir)
    {
    case 0:
      st->maze[i][j] &= ~WALL_TOP;
      if(j>0)
	maze[i][j-1] &= ~WALL_BOTTOM;
      break;
    case 1:
      st->maze[i][j] &= ~WALL_RIGHT;
      if(i<st->maze_size_x-1)
	maze[i+1][j] &= ~WALL_LEFT;
      break;
    case 2:
      st->maze[i][j] &= ~WALL_BOTTOM;
      if(j<st->maze_size_y-1)
	maze[i][j+1] &= ~WALL_BOTTOM;
      break;
    case 3:
      st->maze[i][j] &= ~WALL_LEFT;
      if(i>0)
	maze[i-1][j] &= ~WALL_RIGHT;
      break;
    }
}
#endif /* 0 */


static void
draw_solid_square(struct state *st, 
                  int i, int j,          /* draw a solid square in a square */
		  int dir, GC with_gc)
{
  switch (dir) {
  case WALL_TOP:
      XFillRectangle(st->dpy, st->window, with_gc,
		     border_x + st->bw+(st->bw==0?1:0) + st->grid_width * i, 
		     border_y - st->bw-(st->bw==0?1:0) + st->grid_height * j, 
		     st->grid_width - (st->bw+st->bw+(st->bw==0?1:0)), st->grid_height);
      break;
  case WALL_RIGHT:
      XFillRectangle(st->dpy, st->window, with_gc,
		     border_x + st->bw+(st->bw==0?1:0) + st->grid_width * i, 
		     border_y + st->bw+(st->bw==0?1:0) + st->grid_height * j, 
		     st->grid_width, st->grid_height - (st->bw+st->bw+(st->bw==0?1:0)));
      break;
  case WALL_BOTTOM:
      XFillRectangle(st->dpy, st->window, with_gc,
		     border_x + st->bw+(st->bw==0?1:0) + st->grid_width * i, 
		     border_y + st->bw+(st->bw==0?1:0) + st->grid_height * j, 
		     st->grid_width - (st->bw+st->bw+(st->bw==0?1:0)), st->grid_height);
      break;
  case WALL_LEFT:
      XFillRectangle(st->dpy, st->window, with_gc,
		     border_x - st->bw-(st->bw==0?1:0) + st->grid_width * i, 
		     border_y + st->bw+(st->bw==0?1:0) + st->grid_height * j, 
		     st->grid_width, st->grid_height - (st->bw+st->bw+(st->bw==0?1:0)));
      break;
  }
}

static int
longdeadend_p(struct state *st, int x1, int y1, int x2, int y2, int endwall)
{
    int dx = x2 - x1, dy = y2 - y1;
    int sidewalls;

    sidewalls = endwall | (endwall >> 2 | endwall << 2);
    sidewalls = ~sidewalls & WALL_ANY;

    while((st->maze[x2][y2] & WALL_ANY) == sidewalls)
    {
        if (x2 + dx < 0 || x2 + dx >= st->maze_size_x ||
            y2 + dy < 0 || y2 + dy >= st->maze_size_y)
          break;
	x2 += dx;
	y2 += dy;
    }

    if((st->maze[x2][y2] & WALL_ANY) == (sidewalls | endwall))
    {
	endwall = (endwall >> 2 | endwall << 2) & WALL_ANY;
	while(x1 != x2 || y1 != y2)
	{
	    x1 += dx;
	    y1 += dy;
	    draw_solid_square(st, x1, y1, endwall, st->sgc);
	    st->maze[x1][y1] |= SOLVER_VISIT;
	}
	return 1;
    }
    else
	return 0;
}

/* Find all dead regions -- areas from which the goal cannot be reached --
   and mark them visited. */
static void
find_dead_regions(struct state *st)
{
    int xx, yy, flipped;

    /* Find all not SOLVER_VISIT squares bordering NOT_DEAD squares
       and mark them NOT_DEAD also.  Repeat until no more such squares. */
    st->maze[st->start_x][st->start_y] |= NOT_DEAD;
    
    do
    {
	flipped = 0;
	for(xx = 0; xx < st->maze_size_x; xx++)
	    for(yy = 0; yy < st->maze_size_y; yy++)
		if(!(st->maze[xx][yy] & (SOLVER_VISIT | NOT_DEAD))
		   && (   (xx && (st->maze[xx-1][yy] & NOT_DEAD))
		       || (yy && (st->maze[xx][yy-1] & NOT_DEAD))))
		{
		    flipped = 1;
		    st->maze[xx][yy] |= NOT_DEAD;
		}
	for(xx = st->maze_size_x-1; xx >= 0; xx--)
	    for(yy = st->maze_size_y-1; yy >= 0; yy--)
		if(!(st->maze[xx][yy] & (SOLVER_VISIT | NOT_DEAD))
		   && (   (xx != st->maze_size_x-1 && (st->maze[xx+1][yy] & NOT_DEAD))
		       || (yy != st->maze_size_y-1 && (st->maze[xx][yy+1] & NOT_DEAD))))
		{
		    flipped = 1;
		    st->maze[xx][yy] |= NOT_DEAD;
		}
    }
    while(flipped);

    for (yy = 0; yy < st->maze_size_y; yy++)
      for (xx = 0; xx < st->maze_size_x; xx++)
      {
	if (st->maze[xx][yy] & NOT_DEAD)
	  st->maze[xx][yy] &= ~NOT_DEAD;
	else if (!(st->maze[xx][yy] & SOLVER_VISIT))
	{
	  st->maze[xx][yy] |= SOLVER_VISIT;
	  if((xx < st->logo_x || xx > st->logo_x + st->logo_width / st->grid_width) ||
	     (yy < st->logo_y || yy > st->logo_y + st->logo_height / st->grid_height))
	  {
            /* if we are completely surrounded by walls, just draw the
               inside part */
            if ((st->maze[xx][yy] & WALL_ANY) == WALL_ANY)
	      XFillRectangle(st->dpy, st->window, st->ugc,
			     border_x + st->bw + st->grid_width * xx,
			     border_y + st->bw + st->grid_height * yy,
			     st->grid_width - (st->bw+st->bw), st->grid_height - (st->bw+st->bw));
	    else
	    {
	      if (! (st->maze[xx][yy] & WALL_LEFT))
		draw_solid_square(st, xx, yy, WALL_LEFT, st->ugc);
	      if (! (st->maze[xx][yy] & WALL_RIGHT))
		draw_solid_square(st, xx, yy, WALL_RIGHT, st->ugc);
	      if (! (st->maze[xx][yy] & WALL_TOP))
		draw_solid_square(st, xx, yy, WALL_TOP, st->ugc);
	      if (! (st->maze[xx][yy] & WALL_BOTTOM))
		draw_solid_square(st, xx, yy, WALL_BOTTOM, st->ugc);
	    }
	  }
	}
      }
}

static int
solve_maze (struct state *st)                     /* solve it with graphical feedback */
{
  struct solve_state *ss = st->solve_state;
  if (!ss)
    ss = st->solve_state = (struct solve_state *) calloc (1, sizeof(*ss));

  if (!ss->running) {
    /* plug up the surrounding wall */
    st->maze[st->end_x][st->end_y] |= (WALL_TOP >> st->end_dir);
    
    /* initialize search path */
    ss->i = 0;
    st->path[ss->i].x = st->end_x;
    st->path[ss->i].y = st->end_y;
    st->path[ss->i].dir = 0;
    st->maze[st->end_x][st->end_y] |= SOLVER_VISIT;

    ss->running = 1;
  }
    
    /* do it */
    /* while (1) */
    {
        int dir, from, ways;

	if ( st->maze[st->path[ss->i].x][st->path[ss->i].y] & START_SQUARE )
          {
            ss->running = 0;
	    return 1;
          }

	if(!st->path[ss->i].dir)
	{
	    ways = 0;
	    /* First visit this square.  Which adjacent squares are open? */
	    for(dir = WALL_TOP; dir & WALL_ANY; dir >>= 1)
	    {
		if(st->maze[st->path[ss->i].x][st->path[ss->i].y] & dir)
		    continue;
		
		ss->y = st->path[ss->i].y - !!(dir & WALL_TOP) + !!(dir & WALL_BOTTOM);
		ss->x = st->path[ss->i].x + !!(dir & WALL_RIGHT) - !!(dir & WALL_LEFT);
		
		if(st->maze[ss->x][ss->y] & SOLVER_VISIT)
                  continue;
		
		from = (dir << 2 & WALL_ANY) | (dir >> 2 & WALL_ANY);
		/* don't enter obvious dead ends */
		if(((st->maze[ss->x][ss->y] & WALL_ANY) | from) != WALL_ANY)
		{
		    if(!longdeadend_p(st, st->path[ss->i].x, st->path[ss->i].y, ss->x, ss->y, dir))
			ways |= dir;
		}
		else
		{
		    draw_solid_square(st, ss->x, ss->y, from, st->sgc);
		    st->maze[ss->x][ss->y] |= SOLVER_VISIT;
		}
	    }
	}
	else
	    ways = st->path[ss->i].ways;
	/* ways now has a bitmask of open paths. */
	
	if(!ways)
	    goto backtrack;

	if (!st->ignorant_p)
	  {
	    ss->x = st->path[ss->i].x - st->start_x;
	    ss->y = st->path[ss->i].y - st->start_y;
	    /* choice one */
	    if(abs(ss->y) <= abs(ss->x))
	      dir = (ss->x > 0) ? WALL_LEFT : WALL_RIGHT;
	    else
	      dir = (ss->y > 0) ? WALL_TOP : WALL_BOTTOM;
	    
	    if(dir & ways)
	      goto found;
	    
	    /* choice two */
	    switch(dir)
	      {
	      case WALL_LEFT:
	      case WALL_RIGHT:
		dir = (ss->y > 0) ? WALL_TOP : WALL_BOTTOM; break;
	      case WALL_TOP:
	      case WALL_BOTTOM:
		dir = (ss->x > 0) ? WALL_LEFT : WALL_RIGHT;
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
	ss->bt = 0;
	ways &= ~dir;  /* tried this one */
	
	ss->y = st->path[ss->i].y - !!(dir & WALL_TOP) + !!(dir & WALL_BOTTOM);
	ss->x = st->path[ss->i].x + !!(dir & WALL_RIGHT) - !!(dir & WALL_LEFT);
	
	/* advance in direction dir */
	st->path[ss->i].dir = dir;
	st->path[ss->i].ways = ways;
	draw_solid_square(st, st->path[ss->i].x, st->path[ss->i].y, dir, st->tgc);
	
	ss->i++;
	st->path[ss->i].dir = 0;
	st->path[ss->i].ways = 0;
	st->path[ss->i].x = ss->x;
	st->path[ss->i].y = ss->y;
	st->maze[ss->x][ss->y] |= SOLVER_VISIT;
        return 0;
	/* continue; */

    backtrack:
	if(ss->i == 0)
	{
	    printf("Unsolvable maze.\n");
            ss->running = 0;
	    return 1;
	}

	if(!ss->bt && !st->ignorant_p)
	    find_dead_regions(st);
	ss->bt = 1;
	from = st->path[ss->i-1].dir;
	from = (from << 2 & WALL_ANY) | (from >> 2 & WALL_ANY);
	
	draw_solid_square(st, st->path[ss->i].x, st->path[ss->i].y, from, st->cgc);
	ss->i--;
    }

    return 0;
} 

#if 0
static void
enter_square (int n)                      /* move into a neighboring square */
{
  draw_solid_square( (int)st->path[n].x, (int)st->path[n].y, 
		    (int)st->path[n].dir, st->tgc);
  
  st->path[n+1].dir = -1;
  switch (st->path[n].dir) {
  case 0: st->path[n+1].x = st->path[n].x;
    st->path[n+1].y = st->path[n].y - 1;
    break;
  case 1: st->path[n+1].x = st->path[n].x + 1;
    st->path[n+1].y = st->path[n].y;
    break;
  case 2: st->path[n+1].x = st->path[n].x;
    st->path[n+1].y = st->path[n].y + 1;
    break;
  case 3: st->path[n+1].x = st->path[n].x - 1;
    st->path[n+1].y = st->path[n].y;
    break;
  }
}
#endif /* 0 */


/*
 *  jmr additions for Jamie Zawinski's <jwz@jwz.org> screensaver stuff,
 *  note that the code above this has probably been hacked about in some
 *  arbitrary way.
 */

static const char *maze_defaults[] = {
  ".background:	   black",
  ".foreground:	   white",
  "*gridSize:	   0",
  "*generator:     -1",
  "*maxLength:     5",
  "*bridge:        False",
  "*ignorant:      False",

  "*solveDelay:	   5000",
  "*preDelay:	   2000000",
  "*postDelay:	   4000000",

  "*liveColor:	   #00FF00",
  "*deadColor:	   #880000",
  "*skipColor:     #8B5A00",
  "*surroundColor: #220055",

  0
};

static XrmOptionDescRec maze_options[] = {
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

static int generator = 0;

static void *
maze_init (Display *dpy_arg, Window window_arg)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
# ifdef DO_STIPPLE
  Pixmap gray;
# endif
  int size;
  XWindowAttributes xgwa;
  unsigned long bg, fg, pfg, pbg, sfg, ufg;

  st->dpy = dpy_arg; 
  st->window = window_arg;

  st->stop = 0;
  st->state = 1;
  st->restart = 1;

  st->ifrandom = 0;
  st->ifinit = 1;

  size = get_integer_resource (st->dpy, "gridSize", "Dimension");
  st->solve_delay = get_integer_resource (st->dpy, "solveDelay", "Integer");
  st->pre_solve_delay = get_integer_resource (st->dpy, "preDelay", "Integer");
  st->post_solve_delay = get_integer_resource (st->dpy, "postDelay", "Integer");
  generator = get_integer_resource(st->dpy, "generator", "Integer");
  st->max_length = get_integer_resource(st->dpy, "maxLength", "Integer");
  st->bridge_p = get_boolean_resource(st->dpy, "bridge", "Boolean");
  st->ignorant_p = get_boolean_resource(st->dpy, "ignorant", "Boolean");

  if (!size) st->ifrandom = 1;

  if (size < 2) size = 7 + (random () % 30);
  st->grid_width = st->grid_height = size;
  st->bw = (size > 6 ? 3 : (size-1)/2);

  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  st->x = 0;
  st->y = 0;

  set_maze_sizes (st, xgwa.width, xgwa.height);

  st->gc  = XCreateGC(st->dpy, st->window, 0, 0);
  st->cgc = XCreateGC(st->dpy, st->window, 0, 0);
  st->tgc = XCreateGC(st->dpy, st->window, 0, 0);
  st->sgc = XCreateGC(st->dpy, st->window, 0, 0);
  st->ugc = XCreateGC(st->dpy, st->window, 0, 0);
  st->logo_gc = XCreateGC(st->dpy, st->window, 0, 0);
  st->erase_gc = XCreateGC(st->dpy, st->window, 0, 0);
  
# ifdef DO_STIPPLE
  gray = XCreateBitmapFromData (st->dpy,st->window,gray1_bits,gray1_width,gray1_height);
# endif

  bg  = get_pixel_resource (st->dpy, xgwa.colormap, "background","Background");
  fg  = get_pixel_resource (st->dpy, xgwa.colormap, "foreground","Foreground");
  pfg = get_pixel_resource (st->dpy, xgwa.colormap, "liveColor", "Foreground");
  pbg = get_pixel_resource (st->dpy, xgwa.colormap, "deadColor", "Foreground");
  sfg = get_pixel_resource (st->dpy, xgwa.colormap, "skipColor", "Foreground");
  ufg = get_pixel_resource (st->dpy, xgwa.colormap, "surroundColor", "Foreground");

  XSetForeground (st->dpy, st->gc, fg);
  XSetBackground (st->dpy, st->gc, bg);
  XSetForeground (st->dpy, st->cgc, pbg);
  XSetBackground (st->dpy, st->cgc, bg);
  XSetForeground (st->dpy, st->tgc, pfg);
  XSetBackground (st->dpy, st->tgc, bg);
  XSetForeground (st->dpy, st->sgc, sfg);
  XSetBackground (st->dpy, st->sgc, bg);
  XSetForeground (st->dpy, st->ugc, ufg);
  XSetBackground (st->dpy, st->ugc, bg);
  XSetForeground (st->dpy, st->logo_gc, fg);
  XSetBackground (st->dpy, st->logo_gc, bg);
  XSetForeground (st->dpy, st->erase_gc, bg);
  XSetBackground (st->dpy, st->erase_gc, bg);

# ifdef DO_STIPPLE
  XSetStipple (st->dpy, st->cgc, gray);
  XSetFillStyle (st->dpy, st->cgc, FillOpaqueStippled);
  XSetStipple (st->dpy, st->sgc, gray);
  XSetFillStyle (st->dpy, st->sgc, FillOpaqueStippled);
  XSetStipple (st->dpy, st->ugc, gray);
  XSetFillStyle (st->dpy, st->ugc, FillOpaqueStippled);
# endif
  
  {
    Window r;
    int x, y;
    unsigned int w, h, bbw, d;
    unsigned long *pixels;
    int npixels;
    Pixmap logo_mask = 0;
    st->logo_map = xscreensaver_logo (xgwa.screen, xgwa.visual, st->window,
                                      xgwa.colormap, bg,
                                      &pixels, &npixels, &logo_mask,
                                      xgwa.width > 800);
    if (logo_mask) {
      XSetClipMask (st->dpy, st->logo_gc, logo_mask);
      XFreePixmap (st->dpy, logo_mask);
    }
    if (pixels) free (pixels);
    XGetGeometry (st->dpy, st->logo_map, &r, &x, &y, &w, &h, &bbw, &d);
    st->logo_width = w;
    st->logo_height = h;
  }


  st->restart = 0;
  st->sync_p = 1;

  return st;
}


static void
maze_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->restart = 1;
}


static unsigned long
maze_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int this_delay = st->solve_delay;

  if (st->eraser || st->erase_window)
    {
      st->erase_window = 0;
      st->eraser = erase_window (st->dpy, st->window, st->eraser);
      if (st->eraser)
        this_delay = 10000;
      else {
        this_delay = 1000000;
        if (this_delay > st->pre_solve_delay)
          this_delay = st->pre_solve_delay;
      }
      goto END;
    }

    if (st->restart || st->stop) goto pop;
    switch (st->state) {
    case 1:
      initialize_maze(st);
      if (st->ifrandom && st->ifinit)
       {
	 int size;
         size = 7 + (random () % 30);
         st->grid_width = st->grid_height = size;
         st->bw = (size > 6 ? 3 : (size-1)/2);
         st->ifinit = 0;
	 st->restart = 1;
       }
      break;
    case 2:
      XClearWindow(st->dpy, st->window);
      draw_maze_border(st);
      break;
    case 3:
      st->this_gen = generator;
      if(st->this_gen<0 || st->this_gen>2)
	st->this_gen = random()%3;

      switch(st->this_gen)
	{
	case 0:
	  create_maze(st);
	  break;
	case 1:
	  alt_create_maze(st);
	  break;
	case 2:
	  set_create_maze(st);
	  break;
	}
      break;
    case 4:
      this_delay = st->pre_solve_delay;
      break;
    case 5:
      if (! solve_maze(st))
        --st->state;  /* stay in state 5 */
      break;
    case 6:
      st->erase_window = 1;
      this_delay = st->post_solve_delay;
      st->state = 0 ;
      st->ifinit = 1;
      break;
    default:
      abort ();
    }
    ++st->state;
  pop:
    if (st->restart)
      {
        XWindowAttributes xgwa;
        int size;

        st->restart = 0;
        st->stop = 0;
        st->state = 1;

        st->sync_p = ((random() % 4) != 0);

        size = get_integer_resource (st->dpy, "gridSize", "Dimension");
        if (size < 2) size = 7 + (random () % 30);
        st->grid_width = st->grid_height = size;
        st->bw = (size > 6 ? 3 : (size-1)/2);

        XGetWindowAttributes (st->dpy, st->window, &xgwa);
        set_maze_sizes (st, xgwa.width, xgwa.height);
      }

 END:
    return this_delay;
}


static Bool
maze_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  switch (event->type) 
    {
    case ButtonPress:
      switch (event->xbutton.button) 
        {
        case 2:
          st->stop = !st->stop ;
          if (st->state == 5) st->state = 4 ;
          else {
            st->restart = 1;
            st->stop = 0;
          }
          return True;

        default:
          st->restart = 1 ;
          st->stop = 0 ;
          return True;
        }
      break;

    case Expose:
      st->restart = 1;
      break;

    default:
      break;
    }
  return False;
}


static void
maze_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}

XSCREENSAVER_MODULE ("Maze", maze)
