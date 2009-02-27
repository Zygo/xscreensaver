#ifndef lint
static  char sccsid[] = "@(#)maze.c 1.4 86/12/29 XLOCK";
#endif
/*-
 * maze.c - A maze for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1988 by Sun Microsystems
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 17-Jun-94: HP ANSI C compiler needs a type cast for gray_bits
 *            Richard Lloyd (R.K.Lloyd@csc.liv.ac.uk) 
 * 2-Sep-93: xlock version (David Bagley bagleyd@source.asset.com) 
 * 7-Mar-93: Good ideas from xscreensaver (Jamie Zawinski jwz@lucid.com)
 * 6-Jun-85: Martin Weiss Sun Microsystems 
 */

/* original copyright
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

#include <stdio.h>
#include "xlock.h"
#include <X11/bitmaps/gray1>
#include "mazeicon.bit"

#define MIN_MAZE_SIZE	3
#define MAX_SIZE        40
#define MIN_SIZE        7

/* The following limits are adequate for displays up to 2048x2048 */
#define MAX_MAZE_SIZE_X	300
#define MAX_MAZE_SIZE_Y	300

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

#define get_random(x)	(random() % (x))

static XImage logo = {
    0, 0,                       /* width, height */
    0, XYBitmap, 0,             /* xoffset, format, data */
    LSBFirst, 8,                /* byte-order, bitmap-unit */
    LSBFirst, 8, 1              /* bitmap-bit-order, bitmap-pad, depth */
};

typedef struct {
	u_char x;
	u_char y;
	u_char dir;
	} paths;

typedef struct {
        int maze_size_x, maze_size_y, border_x, border_y;
        int sqnum, cur_sq_x, cur_sq_y, path_length;
        int start_x, start_y, start_dir, end_x, end_y, end_dir;
        int logo_x, logo_y;
        int width, height, counter;
	int sq_size_x, sq_size_y, logo_size_x, logo_size_y;
        GC gray_GC;
        u_short maze[MAX_MAZE_SIZE_X][MAX_MAZE_SIZE_Y];
        paths move_list[MOVE_LIST_SIZE];
        paths save_path[MOVE_LIST_SIZE], path[MOVE_LIST_SIZE];
        } mazestruct;

static mazestruct mazes[MAXSCREENS];

static int choose_door();
static int backup();
static void draw_wall();
static void draw_solid_square();
static void enter_square();

void
initmaze(win)
    Window win;
{
    XWindowAttributes xgwa;
    Pixmap gray;
    mazestruct *mp = &mazes[screen];

    logo.data = (char *) mazeicon_bits;
    logo.width = mazeicon_width;
    logo.height = mazeicon_height;
    logo.bytes_per_line = (mazeicon_width + 7) / 8;
    
    XGetWindowAttributes(dsp, win, &xgwa);
    mp->width = xgwa.width;
    mp->height = xgwa.height;
    mp->gray_GC = XCreateGC(dsp, win, 0, 0);
    gray = XCreateBitmapFromData(dsp, win,
        (char *) gray1_bits, gray1_width, gray1_height);
    XSetBackground(dsp, mp->gray_GC, BlackPixel(dsp, screen));
    XSetStipple(dsp, mp->gray_GC, gray);
    XSetFillStyle(dsp, mp->gray_GC, FillOpaqueStippled);
    mp->counter = 0;
}


static void
clear_window(win) /* blank the window */
        Window win;
{
        mazestruct *mp = &mazes[screen];

        XSetForeground(dsp, Scr[screen].gc, BlackPixel(dsp, screen));
        XFillRectangle(dsp, win, Scr[screen].gc, 0, 0, mp->width, mp->height);
} /* end of clear_window() */


static void
set_maze_sizes()
{
        mazestruct *mp = &mazes[screen];
	/* batchcount is the upper bound on the size */
	if (batchcount > MAX_SIZE || batchcount <= MIN_SIZE)
          batchcount = MAX_SIZE;
        mp->sq_size_x = get_random(batchcount - MIN_SIZE) + MIN_SIZE;
        /*mp->sq_size_x = batchcount;*//* 10 is a good size */
        mp->sq_size_y = mp->sq_size_x;
        mp->logo_size_x = logo.width / mp->sq_size_x + 1;
        mp->logo_size_y = logo.height / mp->sq_size_y + 1;
	
        mp->border_x = mp->border_y = 1;
	mp->maze_size_x = ( mp->width - mp->border_x) / mp->sq_size_x;
	mp->maze_size_y = ( mp->height - mp->border_y ) / mp->sq_size_y;

	if ( mp->maze_size_x < MIN_MAZE_SIZE ||
   	       mp->maze_size_y < MIN_MAZE_SIZE) {
            mp->sq_size_y = mp->sq_size_x = 10;
	    mp->maze_size_x = ( mp->width - mp->border_x) / mp->sq_size_x;
	    mp->maze_size_y = ( mp->height - mp->border_y ) / mp->sq_size_y;
	    if ( mp->maze_size_x < MIN_MAZE_SIZE ||
   	       mp->maze_size_y < MIN_MAZE_SIZE) {
	      (void) fprintf(stderr, "maze too small, exitting\n");
              exit(1);
            }
        }
	mp->border_x = (mp->width - mp->maze_size_x * mp->sq_size_x) / 2;
	mp->border_y = (mp->height - mp->maze_size_y * mp->sq_size_y) / 2;
} /* end of set_maze_sizes */


static void
initialize_maze() /* draw the surrounding wall and start/end squares */
{
        mazestruct *mp = &mazes[screen];
	register int i, j, wall;

    if (Scr[screen].npixels == 2) {
        XSetForeground(dsp, Scr[screen].gc, WhitePixel(dsp, screen));
        XSetForeground(dsp, mp->gray_GC, WhitePixel(dsp, screen));
    }
    if (Scr[screen].npixels > 2) {
        i = random() % Scr[screen].npixels;
        XSetForeground(dsp, Scr[screen].gc, Scr[screen].pixels[i]);
        XSetForeground(dsp, mp->gray_GC, Scr[screen].pixels[i]);
    }
       	/* initialize all squares */
	for ( i=0; i<mp->maze_size_x; i++) {
		for ( j=0; j<mp->maze_size_y; j++) {
			mp->maze[i][j] = 0;
		}
	}

	/* top wall */
	for ( i=0; i<mp->maze_size_x; i++ ) {
		mp->maze[i][0] |= WALL_TOP;
	}

	/* right wall */
	for ( j=0; j<mp->maze_size_y; j++ ) {
		mp->maze[mp->maze_size_x-1][j] |= WALL_RIGHT;
	}

	/* bottom wall */
	for ( i=0; i<mp->maze_size_x; i++ ) {
		mp->maze[i][mp->maze_size_y-1] |= WALL_BOTTOM;
	}

	/* left wall */
	for ( j=0; j<mp->maze_size_y; j++ ) {
		mp->maze[0][j] |= WALL_LEFT;
	}

	/* set start square */
	wall = get_random(4);
	switch (wall) {
		case 0:
			i = get_random(mp->maze_size_x);
			j = 0;
			break;
		case 1:
			i = mp->maze_size_x - 1;
			j = get_random(mp->maze_size_y);
			break;
		case 2:
			i = get_random(mp->maze_size_x);
			j = mp->maze_size_y - 1;
			break;
		case 3:
			i = 0;
			j = get_random(mp->maze_size_y);
			break;
	}
	mp->maze[i][j] |= START_SQUARE;
	mp->maze[i][j] |= ( DOOR_IN_TOP >> wall );
	mp->maze[i][j] &= ~( WALL_TOP >> wall );
	mp->cur_sq_x = i;
	mp->cur_sq_y = j;
	mp->start_x = i;
	mp->start_y = j;
	mp->start_dir = wall;
	mp->sqnum = 0;

        /* set end square */
        wall = (wall + 2)%4;
        switch (wall) {
                case 0:
			i = get_random(mp->maze_size_x);
                        j = 0;
                        break;
                case 1:
                        i = mp->maze_size_x - 1;
			j = get_random(mp->maze_size_y);
                        break;
                case 2:
			i = get_random(mp->maze_size_x);
                        j = mp->maze_size_y - 1;
                        break;
                case 3:
                        i = 0;
			j = get_random(mp->maze_size_y);
                        break;
        }
        mp->maze[i][j] |= END_SQUARE;
        mp->maze[i][j] |= ( DOOR_OUT_TOP >> wall );
	mp->maze[i][j] &= ~( WALL_TOP >> wall );
	mp->end_x = i;
	mp->end_y = j;
	mp->end_dir = wall;

	/* set logo */
	if ( (mp->maze_size_x > mp->logo_size_x + 6) &&
	     (mp->maze_size_y > mp->logo_size_y + 6) ) {
	   mp->logo_x = get_random(mp->maze_size_x - mp->logo_size_x - 6) + 3;
	   mp->logo_y = get_random(mp->maze_size_y - mp->logo_size_y - 6) + 3;

	   for (i=0; i<mp->logo_size_x; i++)
	        for (j=0; j<mp->logo_size_y; j++)
	            mp->maze[mp->logo_x + i][mp->logo_y + j] |= DOOR_IN_TOP;
	}
	else
		mp->logo_y = mp->logo_x = -1;
}


static void
create_maze(win) /* create a maze layout given the intiialized maze */
        Window win;
{
        mazestruct *mp = &mazes[screen];
	register int i, newdoor;

	while (1) {
	    mp->move_list[mp->sqnum].x = mp->cur_sq_x;
	    mp->move_list[mp->sqnum].y = mp->cur_sq_y;
  	    mp->move_list[mp->sqnum].dir = -1;
	    while ( ( newdoor = choose_door(win) ) == -1 ) /* pick a door */
		if ( backup() == -1 ) /* no more doors ... backup */
	  	    return; /* done ... return */

		/* mark the out door */
	    mp->maze[mp->cur_sq_x][mp->cur_sq_y] |= ( DOOR_OUT_TOP >> newdoor );

	    switch (newdoor) {
			case 0: mp->cur_sq_y--;
				break;
			case 1: mp->cur_sq_x++;
				break;
			case 2: mp->cur_sq_y++;
				break;
			case 3: mp->cur_sq_x--;
				break;
		}
	    mp->sqnum++;

	    /* mark the in door */
	    mp->maze[mp->cur_sq_x][mp->cur_sq_y] |=
	       ( DOOR_IN_TOP >> ((newdoor+2)%4) );

	    /* if end square set path length and save path */
	    if ( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & END_SQUARE ) {
			mp->path_length = mp->sqnum;
			for ( i=0; i<mp->path_length; i++) {
				mp->save_path[i].x = mp->move_list[i].x;
				mp->save_path[i].y = mp->move_list[i].y;
				mp->save_path[i].dir = mp->move_list[i].dir;
			}
	    }

	}

} /* end of create_maze() */


static int
choose_door(win) /* pick a new path */
        Window win;
{
        mazestruct *mp = &mazes[screen];
	int candidates[3];
	register int num_candidates;

	num_candidates = 0;

	/* top wall */
	if ((!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & DOOR_IN_TOP )) &&
	    (!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & DOOR_OUT_TOP )) &&
	    (!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & WALL_TOP ))) {
            if ( mp->maze[mp->cur_sq_x][mp->cur_sq_y - 1] & DOOR_IN_ANY ) {
		mp->maze[mp->cur_sq_x][mp->cur_sq_y] |= WALL_TOP;
		mp->maze[mp->cur_sq_x][mp->cur_sq_y - 1] |= WALL_BOTTOM;
		draw_wall(win, mp->cur_sq_x, mp->cur_sq_y, 0);
	    } else
        	candidates[num_candidates++] = 0;
        }

	/* right wall */
	if ((!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & DOOR_IN_RIGHT )) &&
	    (!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & DOOR_OUT_RIGHT )) &&
	    (!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & WALL_RIGHT ))) {
            if ( mp->maze[mp->cur_sq_x + 1][mp->cur_sq_y] & DOOR_IN_ANY ) {
		mp->maze[mp->cur_sq_x][mp->cur_sq_y] |= WALL_RIGHT;
		mp->maze[mp->cur_sq_x + 1][mp->cur_sq_y] |= WALL_LEFT;
		draw_wall(win, mp->cur_sq_x, mp->cur_sq_y, 1);
	    } else
	        candidates[num_candidates++] = 1;
        }

	/* bottom wall */
	if ((!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & DOOR_IN_BOTTOM )) &&
	    (!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & DOOR_OUT_BOTTOM )) &&
	    (!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & WALL_BOTTOM ))) {
	    if ( mp->maze[mp->cur_sq_x][mp->cur_sq_y + 1] & DOOR_IN_ANY ) {
		mp->maze[mp->cur_sq_x][mp->cur_sq_y] |= WALL_BOTTOM;
		mp->maze[mp->cur_sq_x][mp->cur_sq_y + 1] |= WALL_TOP;
		draw_wall(win, mp->cur_sq_x, mp->cur_sq_y, 2);
	    } else
		candidates[num_candidates++] = 2;
        }

	/* left wall */
	if ((!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & DOOR_IN_LEFT )) &&
	    (!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & DOOR_OUT_LEFT )) &&
	    (!( mp->maze[mp->cur_sq_x][mp->cur_sq_y] & WALL_LEFT ))) {
	    if ( mp->maze[mp->cur_sq_x - 1][mp->cur_sq_y] & DOOR_IN_ANY ) {
		mp->maze[mp->cur_sq_x][mp->cur_sq_y] |= WALL_LEFT;
		mp->maze[mp->cur_sq_x - 1][mp->cur_sq_y] |= WALL_RIGHT;
		draw_wall(win, mp->cur_sq_x, mp->cur_sq_y, 3);
	    } else
	        candidates[num_candidates++] = 3;
        }

        /* done wall */
	if (num_candidates == 0)
		return ( -1 );
	if (num_candidates == 1)
		return ( candidates[0] );
	return ( candidates[ get_random(num_candidates) ] );

} /* end of choose_door() */


static int
backup() /* back up a move */
{
        mazestruct *mp = &mazes[screen];
	mp->sqnum--;
	mp->cur_sq_x = mp->move_list[mp->sqnum].x;
	mp->cur_sq_y = mp->move_list[mp->sqnum].y;
	return ( mp->sqnum );
} /* end of backup() */


static void
draw_maze_border(win) /* draw the maze outline */
        Window win;
{
        mazestruct *mp = &mazes[screen];
	register int i, j;

	for ( i=0; i<mp->maze_size_x; i++) {
		if ( mp->maze[i][0] & WALL_TOP ) {
			XDrawLine(dsp, win, Scr[screen].gc,
			    mp->border_x + mp->sq_size_x * i,
			    mp->border_y,
			    mp->border_x + mp->sq_size_x * (i+1),
			    mp->border_y);
		}
		if ((mp->maze[i][mp->maze_size_y - 1] & WALL_BOTTOM)) {
			XDrawLine(dsp, win, Scr[screen].gc,
			    mp->border_x + mp->sq_size_x * i,
			    mp->border_y + mp->sq_size_y * (mp->maze_size_y),
			    mp->border_x + mp->sq_size_x * (i+1),
			    mp->border_y + mp->sq_size_y * (mp->maze_size_y));
		}
	}
	for ( j=0; j<mp->maze_size_y; j++) {
		if ( mp->maze[mp->maze_size_x - 1][j] & WALL_RIGHT ) {
			XDrawLine(dsp, win, Scr[screen].gc,
			    mp->border_x + mp->sq_size_x * mp->maze_size_x,
			    mp->border_y + mp->sq_size_y * j,
			    mp->border_x + mp->sq_size_x * mp->maze_size_x,
			    mp->border_y + mp->sq_size_y * (j+1));
		}
		if ( mp->maze[0][j] & WALL_LEFT ) {
			XDrawLine(dsp, win, Scr[screen].gc,
			    mp->border_x,
			    mp->border_y + mp->sq_size_y * j,
			    mp->border_x,
			    mp->border_y + mp->sq_size_y * (j+1));
		}
	}

	if (mp->logo_x != -1) {
		XPutImage(dsp, win, Scr[screen].gc, &logo,
                        0, 0,
                        mp->border_x + mp->sq_size_x * mp->logo_x +
                          (mp->sq_size_x * mp->logo_size_x - logo.width + 1) /
			  2,
			mp->border_y + mp->sq_size_y * mp->logo_y +
                          (mp->sq_size_y * mp->logo_size_y - logo.height + 1) /
			  2,
			mazeicon_width, mazeicon_height);
	}

	draw_solid_square( win, mp->start_x, mp->start_y, mp->start_dir,
            Scr[screen].gc);
	draw_solid_square( win, mp->end_x, mp->end_y, mp->end_dir,
            Scr[screen].gc);
} /* end of draw_maze() */


static void
draw_wall(win, i, j, dir) /* draw a single wall */
        Window win;
	int i, j, dir;
{
        mazestruct *mp = &mazes[screen];
	switch (dir) {
		case 0:
			XDrawLine(dsp, win, Scr[screen].gc,
				mp->border_x + mp->sq_size_x * i,
				mp->border_y + mp->sq_size_y * j,
				mp->border_x + mp->sq_size_x * (i+1),
				mp->border_y + mp->sq_size_y * j);
			break;
		case 1:
			XDrawLine(dsp, win, Scr[screen].gc,
				mp->border_x + mp->sq_size_x * (i+1),
				mp->border_y + mp->sq_size_y * j,
				mp->border_x + mp->sq_size_x * (i+1),
				mp->border_y + mp->sq_size_y * (j+1));
			break;
		case 2:
			XDrawLine(dsp, win, Scr[screen].gc,
				mp->border_x + mp->sq_size_x * i,
				mp->border_y + mp->sq_size_y * (j+1),
				mp->border_x + mp->sq_size_x * (i+1),
				mp->border_y + mp->sq_size_y * (j+1));
			break;
		case 3:
			XDrawLine(dsp, win, Scr[screen].gc,
				mp->border_x + mp->sq_size_x * i,
				mp->border_y + mp->sq_size_y * j,
				mp->border_x + mp->sq_size_x * i,
				mp->border_y + mp->sq_size_y * (j+1));
			break;
	}
} /* end of draw_wall */


static void
draw_solid_square(win, i, j, dir, gc) /* draw a solid square in a square */
        Window win;
	register int i, j, dir;
	GC gc;
{
        mazestruct *mp = &mazes[screen];
	switch (dir) {
		case 0: XFillRectangle(dsp, win, gc,
			mp->border_x + 3 + mp->sq_size_x * i,
			mp->border_y - 3 + mp->sq_size_y * j,
			mp->sq_size_x - 6, mp->sq_size_y);
			break;
		case 1: XFillRectangle(dsp, win, gc,
			mp->border_x + 3 + mp->sq_size_x * i,
			mp->border_y + 3 + mp->sq_size_y * j,
			mp->sq_size_x, mp->sq_size_y - 6);
			break;
		case 2: XFillRectangle(dsp, win, gc,
			mp->border_x + 3 + mp->sq_size_x * i,
			mp->border_y + 3 + mp->sq_size_y * j,
			mp->sq_size_x - 6, mp->sq_size_y);
			break;
		case 3: XFillRectangle(dsp, win, gc,
			mp->border_x - 3 + mp->sq_size_x * i,
			mp->border_y + 3 + mp->sq_size_y * j,
			mp->sq_size_x, mp->sq_size_y - 6);
			break;
	}

} /* end of draw_solid_square() */


static void
solve_maze(win) /* solve it with graphical feedback */
        Window win;
{
        mazestruct *mp = &mazes[screen];
	int i;


	/* plug up the surrounding wall */
	mp->maze[mp->start_x][mp->start_y] |= (WALL_TOP >> mp->start_dir);
	mp->maze[mp->end_x][mp->end_y] |= (WALL_TOP >> mp->end_dir);

	/* initialize search path */
	i = 0;
	mp->path[i].x = mp->end_x;
	mp->path[i].y = mp->end_y;
	mp->path[i].dir = -1;

	/* do it */
	while (1) {
 	    if ( ++mp->path[i].dir >= 4 ) {
		i--;
		draw_solid_square( win, 
			(int)(mp->path[i].x), (int)(mp->path[i].y),
			(int)(mp->path[i].dir), mp->gray_GC);
	    }
	    else if ( ! (mp->maze[mp->path[i].x][mp->path[i].y] &
			(WALL_TOP >> mp->path[i].dir))  &&
			( (i == 0) || ( (mp->path[i].dir !=
			(mp->path[i-1].dir+2)%4) ) ) ) {
		enter_square(win, i);
		i++;
		if ( mp->maze[mp->path[i].x][mp->path[i].y] & START_SQUARE ) {
			return;
		}
	    }
	}
} /* end of solve_maze() */


static void
enter_square(win, n) /* move into a neighboring square */
        Window win;
	int n;
{
        mazestruct *mp = &mazes[screen];
	draw_solid_square( win, (int)mp->path[n].x, (int)mp->path[n].y,
		(int)mp->path[n].dir, Scr[screen].gc);

	mp->path[n+1].dir = -1;
	switch (mp->path[n].dir) {
		case 0: mp->path[n+1].x = mp->path[n].x;
			mp->path[n+1].y = mp->path[n].y - 1;
			break;
		case 1: mp->path[n+1].x = mp->path[n].x + 1;
			mp->path[n+1].y = mp->path[n].y;
			break;
		case 2: mp->path[n+1].x = mp->path[n].x;
			mp->path[n+1].y = mp->path[n].y + 1;
			break;
		case 3: mp->path[n+1].x = mp->path[n].x - 1;
			mp->path[n+1].y = mp->path[n].y;
			break;
	}


} /* end of enter_square() */

void
drawmaze(win)
    Window win;
{
    mazestruct *mp = &mazes[screen];
    switch (mp->counter)
    {
      case 0:
	clear_window(win);
	set_maze_sizes();
      	initialize_maze();
        break;
      case 1:
      	draw_maze_border(win);
        break;
      case 2:
	create_maze(win);
        break;
      case 3:
      case 4:
	(void) sleep(1);
        break;
      case 5:
	solve_maze(win);
        break;
      case 6:
      case 7:
      case 8:
      case 9:
      	(void) sleep(1);
        break;
    }
    if (mp->counter == 9)
      mp->counter = 0;
    else
      mp->counter++;
} /*  end of drawmaze() */

