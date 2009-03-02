/* -*- mode: C; tab-width: 2 -*-
 * blaster, Copyright (c) 1999 Jonathan H. Lin <jonlin@tesuji.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 *  Robots that move randomly and shoot lasers at each other. If the
 *  mothership is active, it will fly back and forth horizontally, 
 *  firing 8 lasers in the 8 cardinal directions. The explosions are
 *  a 20 frame animation. Robots regenerate after the explosion is finished
 *  and all of its lasers have left the screen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "screenhack.h"

struct laser_state {
	int active;
	int start_x,start_y;
	int end_x, end_y;
};

struct robot_state {
	int alive;
	int death;

	int move_style;
	int target;

	int old_x, old_y;
	int new_x, new_y;

	int radius;
	GC robot_color;
	GC laser_color;
	struct laser_state *lasers;
};

struct mother_ship_state {
	int active;
	int death;
	int old_x,new_x;
	int y;
	GC ship_color;
	GC laser_color;
	struct laser_state *lasers;
};




struct state {
  Display *dpy;
  Window window;

  GC r_color0, r_color1, r_color2, r_color3, r_color4, r_color5, l_color0, l_color1;
  GC s_color;
  GC black;

  int delay;

  int NUM_ROBOTS;
  int NUM_LASERS;

  int MOTHER_SHIP;
  int MOTHER_SHIP_WIDTH;
  int MOTHER_SHIP_HEIGHT;
  int MOTHER_SHIP_LASER;
  int MOTHER_SHIP_PERIOD;
  int MOTHER_SHIP_HITS;

  int LINE_MOVE_STYLE;
  int RANDOM_MOVE_STYLE;
  int NUM_MOVE_STYLES;

  int EXPLODE_SIZE_1;
  int EXPLODE_SIZE_2;
  int EXPLODE_SIZE_3;
  GC EXPLODE_COLOR_1;
  GC EXPLODE_COLOR_2;

  XArc *stars;
  int NUM_STARS;
  int MOVE_STARS;
  int MOVE_STARS_X;
  int MOVE_STARS_Y;
  int MOVE_STARS_RANDOM;

  struct mother_ship_state *mother;

  struct robot_state *robots;

  XWindowAttributes xgwa;

  int initted;

  int draw_x;
  int draw_y;
  int draw_z;
};


/* creates a new robot. It starts out on one of the edges somewhere and
	has no initial velocity. A target is randomly picked. */
static void make_new_robot(struct state *st, int index)
{
	int laser_check = 0;
	int x=0;

	for(x=0;x<st->NUM_LASERS;x++) {
		if(st->robots[index].lasers[x].active) {
			x=st->NUM_LASERS;
			laser_check = 1;
		}
	}
	if(laser_check==0) {
		st->robots[index].alive=1;

		st->robots[index].radius = 7+(random()%7);

		st->robots[index].move_style = random()%st->NUM_MOVE_STYLES;
		if(random()%2==0) {
			st->robots[index].new_x=random()%(st->xgwa.width-st->robots[index].radius);
			st->robots[index].old_x=st->robots[index].new_x;
			if(random()%2==0) {
				st->robots[index].new_y=0;
				st->robots[index].old_y=0;
			}
			else {
				st->robots[index].new_y=st->xgwa.height-st->robots[index].radius;
				st->robots[index].old_y = st->robots[index].new_y;
			}
		}
		else {
			st->robots[index].new_y=random()%(st->xgwa.height-st->robots[index].radius);
			st->robots[index].old_y = st->robots[index].new_y;
			if(random()%2) {
				st->robots[index].new_x=0;
				st->robots[index].old_x=0;
			}
			else {
				st->robots[index].new_x=st->xgwa.width-st->robots[index].radius;
				st->robots[index].old_x=st->robots[index].new_x;
			}
		}
			
		x=random()%6;
		if(x==0) {
			st->robots[index].robot_color = st->r_color0;
		}
		else if(x==1) {
			st->robots[index].robot_color = st->r_color1;
		}
		else if(x==2) {
			st->robots[index].robot_color = st->r_color2;
		}
		else if(x==3) {
			st->robots[index].robot_color = st->r_color3;
		}
		else if(x==4) {
			st->robots[index].robot_color = st->r_color4;
		}
		else if(x==5) {
		 	st->robots[index].robot_color = st->r_color5;
		}

		if(random()%2==0) {
	 		st->robots[index].laser_color = st->l_color0;
		}
		else {
 			st->robots[index].laser_color = st->l_color1;
		}

		if(st->NUM_ROBOTS>1) {
			st->robots[index].target = random()%st->NUM_ROBOTS;
			while(st->robots[index].target==index) {
				st->robots[index].target = random()%st->NUM_ROBOTS;
			}	
		}
	}
}

/* moves each robot, randomly changing its direction and velocity.
	At random a laser is shot toward that robot's target. Also at random
	the target can change. */
static void move_robots(struct state *st)
{
	int x=0;
	int y=0;
	int dx=0;
	int dy=0;
	int target_x = 0;
	int target_y = 0;
	double slope = 0;

	for(x=0;x<st->NUM_ROBOTS;x++) {
		if(st->robots[x].alive) {
			if((st->robots[x].new_x == st->robots[x].old_x) && (st->robots[x].new_y == st->robots[x].old_y)) {
				if(st->robots[x].new_x==0) {
					st->robots[x].old_x = -((random()%3)+1);
				}
				else {
					st->robots[x].old_x = st->robots[x].old_x + (random()%3)+1;
				}
				if(st->robots[x].new_y==0) {
					st->robots[x].old_y = -((random()%3)+1);
				}
				else {
					st->robots[x].old_y = st->robots[x].old_y + (random()%3)+1;
				}
			}
			if(st->robots[x].move_style==st->LINE_MOVE_STYLE) {
				dx = st->robots[x].new_x - st->robots[x].old_x;
				dy = st->robots[x].new_y - st->robots[x].old_y;
				if(dx > 3) {
					dx = 3;
				}
				else if(dx < -3) {
					dx = -3;
				}
				if(dy > 3) {
					dy = 3;
				}
				else if(dy < -3) {
					dy = -3;
				}
				st->robots[x].old_x = st->robots[x].new_x;
				st->robots[x].old_y = st->robots[x].new_y;

				st->robots[x].new_x = st->robots[x].new_x + dx;
				st->robots[x].new_y = st->robots[x].new_y + dy;
			}
			else if(st->robots[x].move_style==st->RANDOM_MOVE_STYLE) {
				dx = st->robots[x].new_x - st->robots[x].old_x;
				dy = st->robots[x].new_y - st->robots[x].old_y;
				y=random()%3;
				if(y==0) {
					dx = dx - ((random()%7)+1);
				}
				else if(y==1){
					dx = dx + ((random()%7)+1);
				}
				else {
					dx = (-1)*dx;
				}
				if(dx > 3) {
					dx = 3;
				}
				else if(dx < -3) {
					dx = -3;
				}

				y = random()%3;
				if(y==0) {
					dy = dy - ((random()%7)+1);
				}
				else if(y==1){
					dy = dy + ((random()%7)+1);
				}
				else {
					dx = (-1)*dx;
				}
				if(dy > 3) {
					dy = 3;
				}
				else if(dy < -3) {
					dy = -3;
				}
				st->robots[x].old_x = st->robots[x].new_x;
				st->robots[x].old_y = st->robots[x].new_y;

				st->robots[x].new_x = st->robots[x].new_x + dx;
				st->robots[x].new_y = st->robots[x].new_y + dy;
			}

			/* bounds corrections */
			if(st->robots[x].new_x >= st->xgwa.width-st->robots[x].radius) {
				st->robots[x].new_x = st->xgwa.width - st->robots[x].radius;
			}
			else if(st->robots[x].new_x < 0) {
				st->robots[x].new_x = 0;
			}
			if(st->robots[x].new_y >= st->xgwa.height-st->robots[x].radius) {
				st->robots[x].new_y = st->xgwa.height - st->robots[x].radius;
			}
			else if(st->robots[x].new_y < 0) {
				st->robots[x].new_y = 0;
			}
		
			if(random()%10==0) {
				st->robots[x].move_style = 1;
			}
			else {
				st->robots[x].move_style = 0;
			}

			if(st->NUM_ROBOTS>1) {
				if(random()%2==0) {
					if(random()%200==0) {
						st->robots[x].target = random()%st->NUM_ROBOTS;
						while(st->robots[x].target==x) {
							st->robots[x].target = random()%st->NUM_ROBOTS;
						}	
						for(y=0;y<st->NUM_LASERS;y++) {
							if(st->robots[x].lasers[y].active == 0) {
								st->robots[x].lasers[y].active = 1;
								if(random()%2==0) {
									if(random()%2==0) {
										st->robots[x].lasers[y].start_x = st->robots[x].new_x+st->robots[x].radius;
										st->robots[x].lasers[y].start_y = st->robots[x].new_y+st->robots[x].radius;
										st->robots[x].lasers[y].end_x = st->robots[x].lasers[y].start_x+7;
										st->robots[x].lasers[y].end_y = st->robots[x].lasers[y].start_y+7;
									}
									else {
										st->robots[x].lasers[y].start_x = st->robots[x].new_x-st->robots[x].radius;
										st->robots[x].lasers[y].start_y = st->robots[x].new_y+st->robots[x].radius;
										st->robots[x].lasers[y].end_x = st->robots[x].lasers[y].start_x-7;
										st->robots[x].lasers[y].end_y = st->robots[x].lasers[y].start_y+7;
									}
								}
								else {
									if(random()%2==0) {
										st->robots[x].lasers[y].start_x = st->robots[x].new_x-st->robots[x].radius;
										st->robots[x].lasers[y].start_y = st->robots[x].new_y-st->robots[x].radius;
										st->robots[x].lasers[y].end_x = st->robots[x].lasers[y].start_x-7;
										st->robots[x].lasers[y].end_y = st->robots[x].lasers[y].start_y-7;
									}
									else {
										st->robots[x].lasers[y].start_x = st->robots[x].new_x+st->robots[x].radius;
										st->robots[x].lasers[y].start_y = st->robots[x].new_y-st->robots[x].radius;
										st->robots[x].lasers[y].end_x = st->robots[x].lasers[y].start_x+7;
										st->robots[x].lasers[y].end_y = st->robots[x].lasers[y].start_y-7;
									}
								}
								y = st->NUM_LASERS;
							}
						}
					}
					else {
						for(y=0;y<st->NUM_LASERS;y++) {
							if(st->robots[x].lasers[y].active==0) {
								target_x = st->robots[st->robots[x].target].new_x;
								target_y = st->robots[st->robots[x].target].new_y;
								if((target_x-st->robots[x].new_x)!=0) {
									slope = ((double)target_y-st->robots[x].new_y)/((double)(target_x-st->robots[x].new_x));

									if((slope<1) && (slope>-1)) {
										if(target_x>st->robots[x].new_x) {
											st->robots[x].lasers[y].start_x = st->robots[x].radius;
											st->robots[x].lasers[y].end_x = st->robots[x].lasers[y].start_x + 7;
										}
										else {
											st->robots[x].lasers[y].start_x = -st->robots[x].radius;
											st->robots[x].lasers[y].end_x = st->robots[x].lasers[y].start_x - 7;
										}
										st->robots[x].lasers[y].start_y = (int)(st->robots[x].lasers[y].start_x * slope);
										st->robots[x].lasers[y].end_y = (int)(st->robots[x].lasers[y].end_x * slope);
									}
									else {
										slope = (target_x-st->robots[x].new_x)/(target_y-st->robots[x].new_y);
										if(target_y>st->robots[x].new_y) {
											st->robots[x].lasers[y].start_y = st->robots[x].radius;
											st->robots[x].lasers[y].end_y = st->robots[x].lasers[y].start_y + 7;
										}
										else {
											st->robots[x].lasers[y].start_y = -st->robots[x].radius;
											st->robots[x].lasers[y].end_y = st->robots[x].lasers[y].start_y - 7;
										}
										st->robots[x].lasers[y].start_x = (int)(st->robots[x].lasers[y].start_y * slope);;
										st->robots[x].lasers[y].end_x = (int)(st->robots[x].lasers[y].end_y * slope);
									}
									st->robots[x].lasers[y].start_x = st->robots[x].lasers[y].start_x + st->robots[x].new_x;
									st->robots[x].lasers[y].start_y = st->robots[x].lasers[y].start_y + st->robots[x].new_y;
									st->robots[x].lasers[y].end_x = st->robots[x].lasers[y].end_x + st->robots[x].new_x;
									st->robots[x].lasers[y].end_y = st->robots[x].lasers[y].end_y + st->robots[x].new_y;
								}
								else {
									if(target_y > st->robots[x].new_y) {
										st->robots[x].lasers[y].start_x = st->robots[x].new_x;
										st->robots[x].lasers[y].start_y = st->robots[x].new_y+st->robots[x].radius;
										st->robots[x].lasers[y].end_x = st->robots[x].new_x;
										st->robots[x].lasers[y].end_y = st->robots[x].lasers[y].start_y+7;
									}
									else {
										st->robots[x].lasers[y].start_x = st->robots[x].new_x;
										st->robots[x].lasers[y].start_y = st->robots[x].new_y-st->robots[x].radius;
										st->robots[x].lasers[y].end_x = st->robots[x].new_x;
										st->robots[x].lasers[y].end_y = st->robots[x].lasers[y].start_y-7;
									}
								}
							
								if((((st->robots[x].lasers[y].start_x - st->robots[x].lasers[y].end_x) > 7) || 
									 ((st->robots[x].lasers[y].end_x - st->robots[x].lasers[y].start_x) > 7)) &&  
									(((st->robots[x].lasers[y].start_y - st->robots[x].lasers[y].end_y) > 7) || 
									 ((st->robots[x].lasers[y].end_y - st->robots[x].lasers[y].start_y) > 7))) {
								}
								else {
									st->robots[x].lasers[y].active = 1;
									y = st->NUM_LASERS;
								}
							}
						}
					}
				}
			}
		}
		else {
			if(st->robots[x].death==0) {
				make_new_robot(st,x);
			}
		}
	}

}

/* This moves a single laser one frame. collisions with other robots or
	the mothership is checked. */
static void move_laser(struct state *st, int rindex, int index)
{
	int x=0;
	int y=0;
	int z=0;
	int dx=0;
	int dy=0;
	struct laser_state *laser;
	if(rindex>=0) {
		laser = st->robots[rindex].lasers;
	}
	else {
		laser = st->mother->lasers;
	}
	if(laser[index].active) {
		/* collision with other robots are checked here */
		for(x=0;x<st->NUM_ROBOTS;x++) {
			if(x!=rindex) {
				if(st->robots[x].alive) {
					y = laser[index].start_x-st->robots[x].new_x;
					if(y<0) {
						y = st->robots[x].new_x-laser[index].start_x;
					}
					z = laser[index].start_y-st->robots[x].new_y;
					if(z<0) {
						z = st->robots[x].new_y-laser[index].start_y;
					}
					if((z<st->robots[x].radius-1)&&(y<st->robots[x].radius-1)) {
						st->robots[x].alive = 0;
						st->robots[x].death = 20;
            if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].old_x, st->robots[x].old_y, st->robots[x].radius, st->robots[x].radius, 0, 360*64);
            if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x, st->robots[x].new_y, st->robots[x].radius, st->robots[x].radius, 0, 360*64);
						laser[index].active = 0;
						x = st->NUM_ROBOTS;
					}
					else {
						y = laser[index].end_x-st->robots[x].new_x;
						if(y<0) {
							y = st->robots[x].new_x-laser[index].end_x;
						}
						z = laser[index].end_y-st->robots[x].new_y;
						if(z<0) {
							z = st->robots[x].new_y-laser[index].end_y;
						}
						if((z<st->robots[x].radius-1)&&(y<st->robots[x].radius-1)) {
							st->robots[x].alive = 0;
							st->robots[x].death = 20;
            if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].old_x, st->robots[x].old_y, st->robots[x].radius, st->robots[x].radius, 0, 360*64);
            if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x, st->robots[x].new_y, st->robots[x].radius, st->robots[x].radius, 0, 360*64);
							laser[index].active = 0;
							x = st->NUM_ROBOTS;
						}
					}
				}
			}
		}
		if((st->MOTHER_SHIP)&&(rindex!=-1)) {
			if(laser[index].active) {
				if(st->mother->active) {
					y = laser[index].start_x-st->mother->new_x;
					if(y<0) {
						y = st->mother->new_x-laser[index].start_x;
					}
					z = laser[index].start_y-st->mother->y;
					if(z<0) {
						z = st->mother->y-laser[index].start_y;
					}
					if((z<st->MOTHER_SHIP_HEIGHT-1)&&(y<st->MOTHER_SHIP_WIDTH-1)) {
						laser[index].active = 0;
						st->mother->active--;
					}
					else {
						y = laser[index].end_x-st->mother->new_x;
						if(y<0) {
							y = st->mother->new_x-laser[index].end_x;
						}
						z = laser[index].end_y-st->mother->y;
						if(z<0) {
							z = st->mother->y-laser[index].end_y;
						}
						if((z<st->MOTHER_SHIP_HEIGHT-1)&&(y<st->MOTHER_SHIP_WIDTH-1)) {
							laser[index].active = 0;
							st->mother->active--;
						}
					}

					if(st->mother->active==0) {
						st->mother->death=20;
					}
				}
			}
		}

		if(laser[index].active) {
			dx = laser[index].start_x - laser[index].end_x;
			dy = laser[index].start_y - laser[index].end_y;
		
			laser[index].start_x = laser[index].end_x;
			laser[index].start_y = laser[index].end_y;
			laser[index].end_x = laser[index].end_x-dx;
			laser[index].end_y = laser[index].end_y-dy;
			
			if((laser[index].end_x < 0) || (laser[index].end_x >= st->xgwa.width) ||
				(laser[index].end_y < 0) || (laser[index].end_y >= st->xgwa.height)) {
				laser[index].active = 0;
			}				
		}
	}
}

/* All the robots are drawn, including the mother ship and the explosions.
	After all the robots have been drawn, their laser banks are check and
	the active lasers are drawn. */
static void draw_robots(struct state *st)
{
	int x=0;
	int y=0;

	for(x=0;x<st->NUM_ROBOTS;x++) {
		if(st->robots[x].alive) {
      if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].old_x, st->robots[x].old_y, st->robots[x].radius, st->robots[x].radius, 0, 360*64);
			XFillArc(st->dpy, st->window, st->robots[x].robot_color, st->robots[x].new_x, st->robots[x].new_y, st->robots[x].radius, st->robots[x].radius, 0, 360*64);
		}
		else {
			if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].old_x, st->robots[x].old_y, st->robots[x].radius, st->robots[x].radius, 0, 360*64);
			if(st->robots[x].death) {
				if(st->robots[x].death==20) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==18) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==17) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==15) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==14) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==13) {
          if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==12) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==11) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==10) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==9) {
          if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==8) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==7) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->robots[x].death==6) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_2, st->EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(st->robots[x].death==4) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_2, st->EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(st->robots[x].death==3) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_2, st->EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(st->robots[x].death==2) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x+(st->robots[x].radius/3), st->robots[x].new_y+(st->robots[x].radius/3), st->EXPLODE_SIZE_2, st->EXPLODE_SIZE_2, 0, 360*64);
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->robots[x].new_x+(1.7*st->robots[x].radius/2), st->robots[x].new_y+(1.7*st->robots[x].radius/2), st->EXPLODE_SIZE_3, st->EXPLODE_SIZE_3, 0, 360*64);
				}
				else if(st->robots[x].death==1) {
          if (st->black) XFillArc(st->dpy, st->window, st->black, st->robots[x].new_x+(1.7*st->robots[x].radius/2), st->robots[x].new_y+(1.7*st->robots[x].radius/2), st->EXPLODE_SIZE_3, st->EXPLODE_SIZE_3, 0, 360*64);
				}
				st->robots[x].death--;
			}
		}
	}

	for(x=0;x<st->NUM_ROBOTS;x++) {
		for(y=0;y<st->NUM_LASERS;y++) {
			if(st->robots[x].lasers[y].active) {
				if (st->black) XDrawLine(st->dpy, st->window, st->black, st->robots[x].lasers[y].start_x,
							 st->robots[x].lasers[y].start_y,
							 st->robots[x].lasers[y].end_x,
							 st->robots[x].lasers[y].end_y);
				move_laser(st, x, y);
				if(st->robots[x].lasers[y].active) {
					XDrawLine(st->dpy, st->window, st->robots[x].laser_color, st->robots[x].lasers[y].start_x,
								 st->robots[x].lasers[y].start_y,
								 st->robots[x].lasers[y].end_x,
								 st->robots[x].lasers[y].end_y);
				}
				else {
					if (st->black) XDrawLine(st->dpy, st->window, st->black, st->robots[x].lasers[y].start_x,
								 st->robots[x].lasers[y].start_y,
								 st->robots[x].lasers[y].end_x,
								 st->robots[x].lasers[y].end_y);
				}					
			}
		}
	}

	if(st->MOTHER_SHIP) {
		if(st->mother->active) {
			if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->old_x, st->mother->y, st->MOTHER_SHIP_WIDTH, st->MOTHER_SHIP_HEIGHT, 0 , 360*64);
			XFillArc(st->dpy, st->window, st->mother->ship_color, st->mother->new_x, st->mother->y, st->MOTHER_SHIP_WIDTH, st->MOTHER_SHIP_HEIGHT, 0 , 360*64);
		}
		else {
			if(st->mother->death) {
				if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->old_x, st->mother->y, st->MOTHER_SHIP_WIDTH, st->MOTHER_SHIP_HEIGHT, 0 , 360*64);
				if(st->mother->death==20) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==18) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==17) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==15) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==14) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==13) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==12) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==11) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==10) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==9) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==8) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==7) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_1, st->EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(st->mother->death==6) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_2, st->EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(st->mother->death==4) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_2, st->EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(st->mother->death==3) {
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_1, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_2, st->EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(st->mother->death==2) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x+1, st->mother->y+1, st->EXPLODE_SIZE_2, st->EXPLODE_SIZE_2, 0, 360*64);
					XFillArc(st->dpy, st->window, st->EXPLODE_COLOR_2, st->mother->new_x+(1.7*st->MOTHER_SHIP_WIDTH/2), st->mother->y+(1.7*st->MOTHER_SHIP_HEIGHT/2), st->EXPLODE_SIZE_3, st->EXPLODE_SIZE_3, 0, 360*64);
				}
				else if(st->mother->death==1) {
					if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x+(1.7*st->MOTHER_SHIP_WIDTH/2), st->mother->y+(1.7*st->MOTHER_SHIP_HEIGHT/2), st->EXPLODE_SIZE_3, st->EXPLODE_SIZE_3, 0, 360*64);
				}
				st->mother->death--;
			}
		}
		for(y=0;y<8;y++) {
			if(st->mother->lasers[y].active) {
				if (st->black) XDrawLine(st->dpy, st->window, st->black, st->mother->lasers[y].start_x,
							 st->mother->lasers[y].start_y,
							 st->mother->lasers[y].end_x,
							 st->mother->lasers[y].end_y);
				move_laser(st, -1,y);
				if(st->mother->lasers[y].active) {
				XDrawLine(st->dpy, st->window, st->mother->laser_color, st->mother->lasers[y].start_x,
							 st->mother->lasers[y].start_y,
							 st->mother->lasers[y].end_x,
							 st->mother->lasers[y].end_y);
				}
				else {
					if (st->black) XDrawLine(st->dpy, st->window, st->black, st->mother->lasers[y].start_x,
								 st->mother->lasers[y].start_y,
								 st->mother->lasers[y].end_x,
								 st->mother->lasers[y].end_y);
				}
			}
		}
	}
}

static void
init_stars(struct state *st)
{
  if(st->NUM_STARS) {
    if (! st->stars)
      st->stars = (XArc *) malloc (st->NUM_STARS * sizeof(XArc));
    for(st->draw_x=0;st->draw_x<st->NUM_STARS;st->draw_x++) {
      st->stars[st->draw_x].x = random()%st->xgwa.width;
      st->stars[st->draw_x].y = random()%st->xgwa.height;
      st->stars[st->draw_x].width = random()%4 + 1;
      st->stars[st->draw_x].height = st->stars[st->draw_x].width;
      st->stars[st->draw_x].angle1 = 0;
      st->stars[st->draw_x].angle2 = 360 * 64;
    }
  }
}


static unsigned long
blaster_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

#ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
  XClearWindow (dpy, window);
#endif

  if (!st->initted) 
    {
      st->initted = 1;

      st->robots = (struct robot_state *) malloc(st->NUM_ROBOTS * sizeof (struct robot_state));
      for(st->draw_x=0;st->draw_x<st->NUM_ROBOTS;st->draw_x++) {
        st->robots[st->draw_x].alive = 0;
        st->robots[st->draw_x].death = 0;
        st->robots[st->draw_x].lasers = (struct laser_state *) malloc (st->NUM_LASERS * sizeof(struct laser_state));
        for(st->draw_y=0;st->draw_y<st->NUM_LASERS;st->draw_y++) {
          st->robots[st->draw_x].lasers[st->draw_y].active = 0;
        }
      }

      init_stars(st);
    }

 		if(st->NUM_STARS) {
			if(st->MOVE_STARS) {
				if (st->black) XFillArcs(st->dpy,st->window,st->black,st->stars,st->NUM_STARS);
				if(st->MOVE_STARS_RANDOM) {
					st->draw_y = st->MOVE_STARS_X;
					st->draw_z = st->MOVE_STARS_Y;
					if(random()%167==0) {
						st->draw_y = (-1)*st->draw_y;
					}
					if(random()%173==0) {
						st->draw_z = (-1)*st->draw_z;
					}
					if(random()%50==0) {
						if(random()%2) {
							st->draw_y++;
							if(st->draw_y>st->MOVE_STARS_RANDOM) {
								st->draw_y = st->MOVE_STARS_RANDOM;
							}
						}
						else {
							st->draw_y--;
							if(st->draw_y < -(st->MOVE_STARS_RANDOM)) {
								st->draw_y = -(st->MOVE_STARS_RANDOM);
							}
						}
					}
					if(random()%50==0) {
						if(random()%2) {
							st->draw_z++;
							if(st->draw_z>st->MOVE_STARS_RANDOM) {
								st->draw_z = st->MOVE_STARS_RANDOM;
							}
						}
						else {
							st->draw_z--;
							if(st->draw_z < -st->MOVE_STARS_RANDOM) {
								st->draw_z = -st->MOVE_STARS_RANDOM;
							}
						}
					}
					st->MOVE_STARS_X = st->draw_y;
					st->MOVE_STARS_Y = st->draw_z;
					for(st->draw_x=0;st->draw_x<st->NUM_STARS;st->draw_x++) {
						st->stars[st->draw_x].x = st->stars[st->draw_x].x + st->draw_y;
						st->stars[st->draw_x].y = st->stars[st->draw_x].y + st->draw_z;
						if(st->stars[st->draw_x].x<0) {
							st->stars[st->draw_x].x = st->stars[st->draw_x].x + st->xgwa.width;
						}
						else if(st->stars[st->draw_x].x>st->xgwa.width) {
							st->stars[st->draw_x].x = st->stars[st->draw_x].x - st->xgwa.width;
						}
						if(st->stars[st->draw_x].y<0) {
							st->stars[st->draw_x].y = st->stars[st->draw_x].y + st->xgwa.height;
						}
						else if(st->stars[st->draw_x].y>st->xgwa.height) {
							st->stars[st->draw_x].y = st->stars[st->draw_x].y - st->xgwa.height;
						}
					}
				}
				else {
					for(st->draw_x=0;st->draw_x<st->NUM_STARS;st->draw_x++) {
						st->stars[st->draw_x].x = st->stars[st->draw_x].x + st->MOVE_STARS_X;
						st->stars[st->draw_x].y = st->stars[st->draw_x].y + st->MOVE_STARS_Y;
						if(st->stars[st->draw_x].x<0) {
							st->stars[st->draw_x].x = st->stars[st->draw_x].x + st->xgwa.width;
						}
						else if(st->stars[st->draw_x].x>st->xgwa.width) {
							st->stars[st->draw_x].x = st->stars[st->draw_x].x - st->xgwa.width;
						}
						if(st->stars[st->draw_x].y<0) {
							st->stars[st->draw_x].y = st->stars[st->draw_x].y + st->xgwa.height;
						}
						else if(st->stars[st->draw_x].y>st->xgwa.height) {
							st->stars[st->draw_x].y = st->stars[st->draw_x].y - st->xgwa.height;
						}
					}
				}
				XFillArcs(st->dpy,st->window,st->s_color,st->stars,st->NUM_STARS);
			}
			else {
				XFillArcs(st->dpy,st->window,st->s_color,st->stars,st->NUM_STARS);
			}
		}

		if(st->MOTHER_SHIP) {
			if(random()%st->MOTHER_SHIP_PERIOD==0) {
				if((st->mother->active==0)&&(st->mother->death==0)) {
					st->mother->active = st->MOTHER_SHIP_HITS;
					st->mother->y = random()%(st->xgwa.height-7);
					if(random()%2==0) {
						st->mother->old_x=0;
						st->mother->new_x=0;
					}
					else {
						st->mother->old_x=st->xgwa.width-25;
						st->mother->new_x=st->xgwa.width-25;
					}
				}
			}
		}
		move_robots(st);
		if(st->MOTHER_SHIP) {
			if(st->mother->active) {
				if(st->mother->old_x==st->mother->new_x) {
					if(st->mother->old_x==0) {
						st->mother->new_x=3;
					}
					else {
						st->mother->new_x=st->mother->new_x-3;
					}
				}
				else {
					if(st->mother->old_x>st->mother->new_x) {
						st->mother->old_x = st->mother->new_x;
						st->mother->new_x = st->mother->new_x-3;
						if(st->mother->new_x<0) {
							st->mother->active=0;
							if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->old_x, st->mother->y, st->MOTHER_SHIP_WIDTH, st->MOTHER_SHIP_HEIGHT, 0 , 360*64);
							if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x, st->mother->y, st->MOTHER_SHIP_WIDTH, st->MOTHER_SHIP_HEIGHT, 0 , 360*64);
						}
					}
					else {
						st->mother->old_x = st->mother->new_x;
						st->mother->new_x = st->mother->new_x+3;
						if(st->mother->new_x>st->xgwa.width) {
							st->mother->active=0;
							if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->old_x, st->mother->y, st->MOTHER_SHIP_WIDTH, st->MOTHER_SHIP_HEIGHT, 0 , 360*64);
							if (st->black) XFillArc(st->dpy, st->window, st->black, st->mother->new_x, st->mother->y, st->MOTHER_SHIP_WIDTH, st->MOTHER_SHIP_HEIGHT, 0 , 360*64);
						}
					}
				}
				st->draw_y=0;
				for(st->draw_x=0;st->draw_x<8;st->draw_x++) {
					if(st->mother->lasers[st->draw_x].active) {
						st->draw_y=1;
						st->draw_x=8;
					}
				}
				if(st->draw_y==0) {
					for(st->draw_x=0;st->draw_x<8;st->draw_x++) {
						st->mother->lasers[st->draw_x].active = 1;
						st->mother->lasers[st->draw_x].start_x=st->mother->new_x+(st->MOTHER_SHIP_WIDTH/2);
						st->mother->lasers[st->draw_x].start_y=st->mother->y+(st->MOTHER_SHIP_HEIGHT/2);
					}
					st->draw_y = (int)(st->MOTHER_SHIP_LASER/1.5);
					st->mother->lasers[0].end_x=st->mother->lasers[0].start_x-st->MOTHER_SHIP_LASER;
					st->mother->lasers[0].end_y=st->mother->lasers[0].start_y;
					st->mother->lasers[1].end_x=st->mother->lasers[1].start_x-st->draw_y;
					st->mother->lasers[1].end_y=st->mother->lasers[1].start_y-st->draw_y;
					st->mother->lasers[2].end_x=st->mother->lasers[2].start_x;
					st->mother->lasers[2].end_y=st->mother->lasers[2].start_y-st->MOTHER_SHIP_LASER;
					st->mother->lasers[3].end_x=st->mother->lasers[3].start_x+st->draw_y;
					st->mother->lasers[3].end_y=st->mother->lasers[3].start_y-st->draw_y;
					st->mother->lasers[4].end_x=st->mother->lasers[4].start_x+st->MOTHER_SHIP_LASER;
					st->mother->lasers[4].end_y=st->mother->lasers[4].start_y;
					st->mother->lasers[5].end_x=st->mother->lasers[5].start_x+st->draw_y;
					st->mother->lasers[5].end_y=st->mother->lasers[5].start_y+st->draw_y;
					st->mother->lasers[6].end_x=st->mother->lasers[6].start_x;
					st->mother->lasers[6].end_y=st->mother->lasers[6].start_y+st->MOTHER_SHIP_LASER;
					st->mother->lasers[7].end_x=st->mother->lasers[7].start_x-st->draw_y;
					st->mother->lasers[7].end_y=st->mother->lasers[7].start_y+st->draw_y;
				}
			}
		}
		draw_robots(st);

    return st->delay;
}

static void *
blaster_init (Display *d, Window w)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
	XGCValues gcv;
	Colormap cmap;
	unsigned long bg;

	st->dpy = d;
	st->window = w;
	XGetWindowAttributes(st->dpy, st->window, &st->xgwa);
	cmap = st->xgwa.colormap;

  st->NUM_ROBOTS=5;
  st->NUM_LASERS=3;

  st->MOTHER_SHIP_WIDTH=25;
  st->MOTHER_SHIP_HEIGHT=7;
  st->MOTHER_SHIP_LASER=15;
  st->MOTHER_SHIP_PERIOD=150;
  st->MOTHER_SHIP_HITS=10;

  st->RANDOM_MOVE_STYLE=1;
  st->NUM_MOVE_STYLES=2;

  st->EXPLODE_SIZE_1=27;
  st->EXPLODE_SIZE_2=19;
  st->EXPLODE_SIZE_3=7;


	st->delay = get_integer_resource(st->dpy, "delay", "Integer");
	if(st->delay==0) {
		st->delay=10000;
	}
	st->NUM_ROBOTS = get_integer_resource(st->dpy, "num_robots","Integer");
	if(st->NUM_ROBOTS==0) {
		st->NUM_ROBOTS=5;
	}
	st->NUM_LASERS = get_integer_resource(st->dpy, "num_lasers","Integer");
	st->EXPLODE_SIZE_1 = get_integer_resource(st->dpy, "explode_size_1","Integer");
	st->EXPLODE_SIZE_2 = get_integer_resource(st->dpy, "explode_size_2","Integer");
	st->EXPLODE_SIZE_3 = get_integer_resource(st->dpy, "explode_size_3","Integer");

	st->NUM_STARS = get_integer_resource(st->dpy, "num_stars","Integer");
	if(get_boolean_resource(st->dpy, "move_stars","Boolean")) {
		st->MOVE_STARS = 1;
		st->MOVE_STARS_X = get_integer_resource(st->dpy, "move_stars_x","Integer");
		st->MOVE_STARS_Y = get_integer_resource(st->dpy, "move_stars_y","Integer");
		st->MOVE_STARS_RANDOM = get_integer_resource(st->dpy, "move_stars_random","Integer");
	}
	else {
		st->MOVE_STARS = 0;
	}


	bg = get_pixel_resource(st->dpy, cmap, "background","Background");
	gcv.function = GXcopy;

#define make_gc(color,name) \
	gcv.foreground = get_pixel_resource (st->dpy, cmap, (name), "Foreground"); \
	color = XCreateGC (st->dpy, st->window, GCForeground|GCFunction, &gcv)

	if(mono_p) {
		gcv.foreground = bg;
		st->black = XCreateGC(st->dpy, st->window, GCForeground|GCFunction, &gcv);
		gcv.foreground = get_pixel_resource(st->dpy, cmap, "foreground", "Foreground");
		st->r_color0 = st->r_color1 = st->r_color2 = st->r_color3 = st->r_color4 = st->r_color5 = st->l_color0 = st->l_color1 = st->s_color=
			XCreateGC(st->dpy, st->window, GCForeground|GCFunction, &gcv);
		if(get_boolean_resource(st->dpy, "mother_ship","Boolean")) {
			st->MOTHER_SHIP_WIDTH=get_integer_resource(st->dpy, "mother_ship_width","Integer");
			st->MOTHER_SHIP_HEIGHT=get_integer_resource(st->dpy, "mother_ship_height","Integer");
			st->MOTHER_SHIP_LASER=get_integer_resource(st->dpy, "mother_ship_laser","Integer");
			st->MOTHER_SHIP_PERIOD=get_integer_resource(st->dpy, "mother_ship_period","Integer");
			st->MOTHER_SHIP_HITS=get_integer_resource(st->dpy, "mother_ship_hits","Integer");
			st->MOTHER_SHIP=1;
			st->mother = (struct mother_ship_state *) malloc(sizeof(struct mother_ship_state));
			st->mother->lasers = (struct laser_state *) malloc(8*sizeof(struct laser_state));
			st->mother->active = 0;
			st->mother->death = 0;
			st->mother->ship_color = st->r_color0;
			st->mother->laser_color = st->r_color0;
		}
	}
	else {
		if(get_boolean_resource(st->dpy, "mother_ship","Boolean")) {
			st->MOTHER_SHIP_WIDTH=get_integer_resource(st->dpy, "mother_ship_width","Integer");
			st->MOTHER_SHIP_HEIGHT=get_integer_resource(st->dpy, "mother_ship_height","Integer");
			st->MOTHER_SHIP_LASER=get_integer_resource(st->dpy, "mother_ship_laser","Integer");
			st->MOTHER_SHIP_PERIOD=get_integer_resource(st->dpy, "mother_ship_period","Integer");
			st->MOTHER_SHIP_HITS=get_integer_resource(st->dpy, "mother_ship_hits","Integer");
			st->MOTHER_SHIP=1;
			st->mother = (struct mother_ship_state *) malloc(sizeof(struct mother_ship_state));
			st->mother->lasers = (struct laser_state *) malloc(8*sizeof(struct laser_state));
			st->mother->active = 0;
			st->mother->death = 0;
			make_gc(st->mother->ship_color,"mother_ship_color0");
			make_gc(st->mother->laser_color,"mother_ship_color1");		
		}

		make_gc (st->s_color,"star_color");
		
		make_gc (st->EXPLODE_COLOR_1,"explode_color_1");
		make_gc (st->EXPLODE_COLOR_2,"explode_color_2");
		
      make_gc (st->r_color0,"r_color0");
      make_gc (st->r_color1,"r_color1");
      make_gc (st->r_color2,"r_color2");
      make_gc (st->r_color3,"r_color3");
      make_gc (st->r_color4,"r_color4");
      make_gc (st->r_color5,"r_color5");
      make_gc (st->l_color0,"l_color0");
      make_gc (st->l_color1,"l_color1");
#ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
    st->black = 0;
#else
		make_gc (st->black,"background");
#endif
	}

  return st;
}


static void
blaster_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
	XGetWindowAttributes (dpy, window, &st->xgwa);
  XClearWindow (dpy, window);
  init_stars (st);
}

static Bool
blaster_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
blaster_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;
  if (st->r_color0) XFreeGC (dpy, st->r_color0);
  if (st->r_color1) XFreeGC (dpy, st->r_color1);
  if (st->r_color2) XFreeGC (dpy, st->r_color2);
  if (st->r_color3) XFreeGC (dpy, st->r_color3);
  if (st->r_color4) XFreeGC (dpy, st->r_color4);
  if (st->r_color5) XFreeGC (dpy, st->r_color5);
  if (st->l_color0) XFreeGC (dpy, st->l_color0);
  if (st->l_color1) XFreeGC (dpy, st->l_color1);
  if (st->s_color)  XFreeGC (dpy, st->s_color);
  if (st->black)    XFreeGC (dpy, st->black);
  if (st->stars) free (st->stars);
  if (st->mother) {
    free (st->mother->lasers);
    free (st->mother);
  }
  for (i = 0; i < st->NUM_ROBOTS; i++)
    free (st->robots[i].lasers);
  free (st->robots);
  free (st);
}


static const char *blaster_defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*r_color0:	#FF00FF",
  "*r_color1:	#FFA500",
  "*r_color2:	#FFFF00",
  "*r_color3:	#FFFFFF",
  "*r_color4:	#0000FF",
  "*r_color5:	#00FFFF",
  "*l_color0:	#00FF00",
  "*l_color1:	#FF0000",
  "*mother_ship_color0:	#00008B",
  "*mother_ship_color1:	#FFFFFF",
  "*explode_color_1: #FFFF00",
  "*explode_color_2: #FFA500",
  "*delay: 10000",
  "*num_robots: 5",
  "*num_lasers: 3",
  "*mother_ship: false",
  "*mother_ship_width: 25",
  "*mother_ship_height: 7",
  "*mother_ship_laser: 15",
  "*mother_ship_period: 150",
  "*mother_ship_hits: 10",  
  "*explode_size_1: 27",
  "*explode_size_2: 19",
  "*explode_size_3: 7",
  "*num_stars: 50",
  "*star_color: white",
  "*move_stars: true",
  "*move_stars_x: 2",
  "*move_stars_y: 1",
  "*move_stars_random: 0",
  0
};

static XrmOptionDescRec blaster_options [] = {
	/* These are the 6 robot colors */
  { "-r_color0",		".r_color0",	XrmoptionSepArg, 0 },
  { "-r_color1",		".r_color1",	XrmoptionSepArg, 0 },
  { "-r_color2",		".r_color2",	XrmoptionSepArg, 0 },
  { "-r_color3",		".r_color3",	XrmoptionSepArg, 0 },
  { "-r_color4",		".r_color4",	XrmoptionSepArg, 0 },
  { "-r_color5",		".r_color5",	XrmoptionSepArg, 0 },
  /* These are the 2 laser colors that robots have */
  { "-l_color0",		".l_color0",	XrmoptionSepArg, 0 },
  { "-l_color1",		".l_color1",	XrmoptionSepArg, 0 },
  /* These are the colors for the mothership and the mothership lasers */
  { "-mother_ship_color0",   ".mother_ship_color0",  XrmoptionSepArg, 0},
  { "-mother_ship_color1",   ".mother_ship_color1",  XrmoptionSepArg, 0},
  /* These are the two colors of the animated explosion */
  { "-explode_color_1",  ".explode_color_1", 	XrmoptionSepArg, 0 },
  { "-explode_color_2",  ".explode_color_2", 	XrmoptionSepArg, 0 },
  /* This is the delay in the main loop */
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  /* The number of robots and the number of lasers each robot has */
  { "-num_robots",   ".num_robots",  XrmoptionSepArg, 0},
  { "-num_lasers",   ".num_lasers",  XrmoptionSepArg, 0},
  /* If this is set, a mothership will appear, otherwise no mothership */
  { "-mother_ship",   ".mother_ship",  XrmoptionNoArg, "true"},
  { "-no_mother_ship",   ".mother_ship",  XrmoptionNoArg, "false"},
  /* This is the width, height, and laser length of the mothership */
  { "-mother_ship_width",   ".mother_ship_width",  XrmoptionSepArg, 0},
  { "-mother_ship_height",   ".mother_ship_height",  XrmoptionSepArg, 0},
  { "-mother_ship_laser",   ".mother_ship_laser",  XrmoptionSepArg, 0},
  /* This is the period which the mothership comes out, higher period==less often */
  { "-mother_ship_period",   ".mother_ship_period",  XrmoptionSepArg, 0},
  /* This is the number of hits it takes to destroy the mothership */
  { "-mother_ship_hits",   ".mother_ship_hits",  XrmoptionSepArg, 0},
  /* These are the size of the radius of the animated explosions */
  { "-explode_size_1", ".explode_size_1",   XrmoptionSepArg, 0},
  { "-explode_size_2", ".explode_size_2",   XrmoptionSepArg, 0},
  { "-explode_size_3", ".explode_size_3",   XrmoptionSepArg, 0},
  /* This sets the number of stars in the star field, if this is set to 0, there will be no stars */
  { "-num_stars", ".num_stars", XrmoptionSepArg, 0},
  /* This is the color of the stars */
  { "-star_color", ".star_color", XrmoptionSepArg, 0},
  /* If this is true, the stars will move */
  { "-move_stars", ".move_stars", XrmoptionNoArg, "true"},
  /* This is the amount the stars will move in the x and y direction */
  { "-move_stars_x", ".move_stars_x", XrmoptionSepArg, 0},
  { "-move_stars_y", ".move_stars_y", XrmoptionSepArg, 0},
  /* If this is non-zero, the stars will move randomly, but will not move more than this number in 
	  either the x or y direction */
  { "-move_stars_random", ".move_stars_random", XrmoptionSepArg, 0},
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Blaster", blaster)
