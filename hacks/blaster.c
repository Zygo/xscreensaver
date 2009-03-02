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

static Display *dpy;
static Window window;
static GC r_color0, r_color1, r_color2, r_color3, r_color4, r_color5, l_color0, l_color1;
static GC s_color;
static GC black;

static int delay;

static int NUM_ROBOTS=5;
static int NUM_LASERS=3;

static int MOTHER_SHIP=0;
static int MOTHER_SHIP_WIDTH=25;
static int MOTHER_SHIP_HEIGHT=7;
static int MOTHER_SHIP_LASER=15;
static int MOTHER_SHIP_PERIOD=150;
static int MOTHER_SHIP_HITS=10;

static int LINE_MOVE_STYLE=0;
static int RANDOM_MOVE_STYLE=1;
static int NUM_MOVE_STYLES=2;

static int EXPLODE_SIZE_1=27;
static int EXPLODE_SIZE_2=19;
static int EXPLODE_SIZE_3=7;
static GC EXPLODE_COLOR_1;
static GC EXPLODE_COLOR_2;

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

static XArc *stars;
static int NUM_STARS;
static int MOVE_STARS;
static int MOVE_STARS_X;
static int MOVE_STARS_Y;
static int MOVE_STARS_RANDOM;

static struct mother_ship_state *mother;

static struct robot_state *robots;

XWindowAttributes xgwa;




/* creates a new robot. It starts out on one of the edges somewhere and
	has no initial velocity. A target is randomly picked. */
static void make_new_robot(int index)
{
	int laser_check = 0;
	int x=0;

	for(x=0;x<NUM_LASERS;x++) {
		if(robots[index].lasers[x].active) {
			x=NUM_LASERS;
			laser_check = 1;
		}
	}
	if(laser_check==0) {
		robots[index].alive=1;

		robots[index].radius = 7+(random()%7);

		robots[index].move_style = random()%NUM_MOVE_STYLES;
		if(random()%2==0) {
			robots[index].new_x=random()%(xgwa.width-robots[index].radius);
			robots[index].old_x=robots[index].new_x;
			if(random()%2==0) {
				robots[index].new_y=0;
				robots[index].old_y=0;
			}
			else {
				robots[index].new_y=xgwa.height-robots[index].radius;
				robots[index].old_y = robots[index].new_y;
			}
		}
		else {
			robots[index].new_y=random()%(xgwa.height-robots[index].radius);
			robots[index].old_y = robots[index].new_y;
			if(random()%2) {
				robots[index].new_x=0;
				robots[index].old_x=0;
			}
			else {
				robots[index].new_x=xgwa.width-robots[index].radius;
				robots[index].old_x=robots[index].new_x;
			}
		}
			
		x=random()%6;
		if(x==0) {
			robots[index].robot_color = r_color0;
		}
		else if(x==1) {
			robots[index].robot_color = r_color1;
		}
		else if(x==2) {
			robots[index].robot_color = r_color2;
		}
		else if(x==3) {
			robots[index].robot_color = r_color3;
		}
		else if(x==4) {
			robots[index].robot_color = r_color4;
		}
		else if(x==5) {
		 	robots[index].robot_color = r_color5;
		}

		if(random()%2==0) {
	 		robots[index].laser_color = l_color0;
		}
		else {
 			robots[index].laser_color = l_color1;
		}

		if(NUM_ROBOTS>1) {
			robots[index].target = random()%NUM_ROBOTS;
			while(robots[index].target==index) {
				robots[index].target = random()%NUM_ROBOTS;
			}	
		}
	}
}

/* moves each robot, randomly changing its direction and velocity.
	At random a laser is shot toward that robot's target. Also at random
	the target can change. */
static void move_robots(void)
{
	int x=0;
	int y=0;
	int dx=0;
	int dy=0;
	int target_x = 0;
	int target_y = 0;
	double slope = 0;

	for(x=0;x<NUM_ROBOTS;x++) {
		if(robots[x].alive) {
			if((robots[x].new_x == robots[x].old_x) && (robots[x].new_y == robots[x].old_y)) {
				if(robots[x].new_x==0) {
					robots[x].old_x = -((random()%3)+1);
				}
				else {
					robots[x].old_x = robots[x].old_x + (random()%3)+1;
				}
				if(robots[x].new_y==0) {
					robots[x].old_y = -((random()%3)+1);
				}
				else {
					robots[x].old_y = robots[x].old_y + (random()%3)+1;
				}
			}
			if(robots[x].move_style==LINE_MOVE_STYLE) {
				dx = robots[x].new_x - robots[x].old_x;
				dy = robots[x].new_y - robots[x].old_y;
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
				robots[x].old_x = robots[x].new_x;
				robots[x].old_y = robots[x].new_y;

				robots[x].new_x = robots[x].new_x + dx;
				robots[x].new_y = robots[x].new_y + dy;
			}
			else if(robots[x].move_style==RANDOM_MOVE_STYLE) {
				dx = robots[x].new_x - robots[x].old_x;
				dy = robots[x].new_y - robots[x].old_y;
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
				robots[x].old_x = robots[x].new_x;
				robots[x].old_y = robots[x].new_y;

				robots[x].new_x = robots[x].new_x + dx;
				robots[x].new_y = robots[x].new_y + dy;
			}

			/* bounds corrections */
			if(robots[x].new_x >= xgwa.width-robots[x].radius) {
				robots[x].new_x = xgwa.width - robots[x].radius;
			}
			else if(robots[x].new_x < 0) {
				robots[x].new_x = 0;
			}
			if(robots[x].new_y >= xgwa.height-robots[x].radius) {
				robots[x].new_y = xgwa.height - robots[x].radius;
			}
			else if(robots[x].new_y < 0) {
				robots[x].new_y = 0;
			}
		
			if(random()%10==0) {
				robots[x].move_style = 1;
			}
			else {
				robots[x].move_style = 0;
			}

			if(NUM_ROBOTS>1) {
				if(random()%2==0) {
					if(random()%200==0) {
						robots[x].target = random()%NUM_ROBOTS;
						while(robots[x].target==x) {
							robots[x].target = random()%NUM_ROBOTS;
						}	
						for(y=0;y<NUM_LASERS;y++) {
							if(robots[x].lasers[y].active == 0) {
								robots[x].lasers[y].active = 1;
								if(random()%2==0) {
									if(random()%2==0) {
										robots[x].lasers[y].start_x = robots[x].new_x+robots[x].radius;
										robots[x].lasers[y].start_y = robots[x].new_y+robots[x].radius;
										robots[x].lasers[y].end_x = robots[x].lasers[y].start_x+7;
										robots[x].lasers[y].end_y = robots[x].lasers[y].start_y+7;
									}
									else {
										robots[x].lasers[y].start_x = robots[x].new_x-robots[x].radius;
										robots[x].lasers[y].start_y = robots[x].new_y+robots[x].radius;
										robots[x].lasers[y].end_x = robots[x].lasers[y].start_x-7;
										robots[x].lasers[y].end_y = robots[x].lasers[y].start_y+7;
									}
								}
								else {
									if(random()%2==0) {
										robots[x].lasers[y].start_x = robots[x].new_x-robots[x].radius;
										robots[x].lasers[y].start_y = robots[x].new_y-robots[x].radius;
										robots[x].lasers[y].end_x = robots[x].lasers[y].start_x-7;
										robots[x].lasers[y].end_y = robots[x].lasers[y].start_y-7;
									}
									else {
										robots[x].lasers[y].start_x = robots[x].new_x+robots[x].radius;
										robots[x].lasers[y].start_y = robots[x].new_y-robots[x].radius;
										robots[x].lasers[y].end_x = robots[x].lasers[y].start_x+7;
										robots[x].lasers[y].end_y = robots[x].lasers[y].start_y-7;
									}
								}
								y = NUM_LASERS;
							}
						}
					}
					else {
						for(y=0;y<NUM_LASERS;y++) {
							if(robots[x].lasers[y].active==0) {
								target_x = robots[robots[x].target].new_x;
								target_y = robots[robots[x].target].new_y;
								if((target_x-robots[x].new_x)!=0) {
									slope = ((double)target_y-robots[x].new_y)/((double)(target_x-robots[x].new_x));

									if((slope<1) && (slope>-1)) {
										if(target_x>robots[x].new_x) {
											robots[x].lasers[y].start_x = robots[x].radius;
											robots[x].lasers[y].end_x = robots[x].lasers[y].start_x + 7;
										}
										else {
											robots[x].lasers[y].start_x = -robots[x].radius;
											robots[x].lasers[y].end_x = robots[x].lasers[y].start_x - 7;
										}
										robots[x].lasers[y].start_y = (int)(robots[x].lasers[y].start_x * slope);
										robots[x].lasers[y].end_y = (int)(robots[x].lasers[y].end_x * slope);
									}
									else {
										slope = (target_x-robots[x].new_x)/(target_y-robots[x].new_y);
										if(target_y>robots[x].new_y) {
											robots[x].lasers[y].start_y = robots[x].radius;
											robots[x].lasers[y].end_y = robots[x].lasers[y].start_y + 7;
										}
										else {
											robots[x].lasers[y].start_y = -robots[x].radius;
											robots[x].lasers[y].end_y = robots[x].lasers[y].start_y + 7;
										}
										robots[x].lasers[y].start_x = (int)(robots[x].lasers[y].start_y * slope);;
										robots[x].lasers[y].start_x = (int)(robots[x].lasers[y].end_y * slope);
									}
									robots[x].lasers[y].start_x = robots[x].lasers[y].start_x + robots[x].new_x;
									robots[x].lasers[y].start_y = robots[x].lasers[y].start_y + robots[x].new_y;
									robots[x].lasers[y].end_x = robots[x].lasers[y].end_x + robots[x].new_x;
									robots[x].lasers[y].end_y = robots[x].lasers[y].end_y + robots[x].new_y;
								}
								else {
									if(target_y > robots[x].new_y) {
										robots[x].lasers[y].start_x = robots[x].new_x;
										robots[x].lasers[y].start_y = robots[x].new_y+robots[x].radius;
										robots[x].lasers[y].end_x = robots[x].new_x;
										robots[x].lasers[y].end_y = robots[x].lasers[y].start_y+7;
									}
									else {
										robots[x].lasers[y].start_x = robots[x].new_x;
										robots[x].lasers[y].start_y = robots[x].new_y-robots[x].radius;
										robots[x].lasers[y].end_x = robots[x].new_x;
										robots[x].lasers[y].end_y = robots[x].lasers[y].start_y-7;
									}
								}
							
								if((((robots[x].lasers[y].start_x - robots[x].lasers[y].end_x) > 7) || 
									 ((robots[x].lasers[y].end_x - robots[x].lasers[y].start_x) > 7)) &&  
									(((robots[x].lasers[y].start_y - robots[x].lasers[y].end_y) > 7) || 
									 ((robots[x].lasers[y].end_y - robots[x].lasers[y].start_y) > 7))) {
								}
								else {
									robots[x].lasers[y].active = 1;
									y = NUM_LASERS;
								}
							}
						}
					}
				}
			}
		}
		else {
			if(robots[x].death==0) {
				make_new_robot(x);
			}
		}
	}

}

/* This moves a single laser one frame. collisions with other robots or
	the mothership is checked. */
static void move_laser(int rindex, int index)
{
	int x=0;
	int y=0;
	int z=0;
	int dx=0;
	int dy=0;
	struct laser_state *laser;
	if(rindex>=0) {
		laser = robots[rindex].lasers;
	}
	else {
		laser = mother->lasers;
	}
	if(laser[index].active) {
		/* collision with other robots are checked here */
		for(x=0;x<NUM_ROBOTS;x++) {
			if(x!=rindex) {
				if(robots[x].alive) {
					y = laser[index].start_x-robots[x].new_x;
					if(y<0) {
						y = robots[x].new_x-laser[index].start_x;
					}
					z = laser[index].start_y-robots[x].new_y;
					if(z<0) {
						z = robots[x].new_y-laser[index].start_y;
					}
					if((z<robots[x].radius-1)&&(y<robots[x].radius-1)) {
						robots[x].alive = 0;
						robots[x].death = 20;
						XFillArc(dpy, window, black, robots[x].old_x, robots[x].old_y, robots[x].radius, robots[x].radius, 0, 360*64);
						XFillArc(dpy, window, black, robots[x].new_x, robots[x].new_y, robots[x].radius, robots[x].radius, 0, 360*64);
						laser[index].active = 0;
						x = NUM_ROBOTS;
					}
					else {
						y = laser[index].end_x-robots[x].new_x;
						if(y<0) {
							y = robots[x].new_x-laser[index].end_x;
						}
						z = laser[index].end_y-robots[x].new_y;
						if(z<0) {
							z = robots[x].new_y-laser[index].end_y;
						}
						if((z<robots[x].radius-1)&&(y<robots[x].radius-1)) {
							robots[x].alive = 0;
							robots[x].death = 20;
							XFillArc(dpy, window, black, robots[x].old_x, robots[x].old_y, robots[x].radius, robots[x].radius, 0, 360*64);
							XFillArc(dpy, window, black, robots[x].new_x, robots[x].new_y, robots[x].radius, robots[x].radius, 0, 360*64);
							laser[index].active = 0;
							x = NUM_ROBOTS;
						}
					}
				}
			}
		}
		if((MOTHER_SHIP)&&(rindex!=-1)) {
			if(laser[index].active) {
				if(mother->active) {
					y = laser[index].start_x-mother->new_x;
					if(y<0) {
						y = mother->new_x-laser[index].start_x;
					}
					z = laser[index].start_y-mother->y;
					if(z<0) {
						z = mother->y-laser[index].start_y;
					}
					if((z<MOTHER_SHIP_HEIGHT-1)&&(y<MOTHER_SHIP_WIDTH-1)) {
						laser[index].active = 0;
						mother->active--;
					}
					else {
						y = laser[index].end_x-mother->new_x;
						if(y<0) {
							y = mother->new_x-laser[index].end_x;
						}
						z = laser[index].end_y-mother->y;
						if(z<0) {
							z = mother->y-laser[index].end_y;
						}
						if((z<MOTHER_SHIP_HEIGHT-1)&&(y<MOTHER_SHIP_WIDTH-1)) {
							laser[index].active = 0;
							mother->active--;
						}
					}

					if(mother->active==0) {
						mother->death=20;
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
			
			if((laser[index].end_x < 0) || (laser[index].end_x >= xgwa.width) ||
				(laser[index].end_y < 0) || (laser[index].end_y >= xgwa.height)) {
				laser[index].active = 0;
			}				
		}
	}
}

/* All the robots are drawn, including the mother ship and the explosions.
	After all the robots have been drawn, their laser banks are check and
	the active lasers are drawn. */
static void draw_robots(void)
{
	int x=0;
	int y=0;

	for(x=0;x<NUM_ROBOTS;x++) {
		if(robots[x].alive) {
			XFillArc(dpy, window, black, robots[x].old_x, robots[x].old_y, robots[x].radius, robots[x].radius, 0, 360*64);
			XFillArc(dpy, window, robots[x].robot_color, robots[x].new_x, robots[x].new_y, robots[x].radius, robots[x].radius, 0, 360*64);
		}
		else {
			XFillArc(dpy, window, black, robots[x].old_x, robots[x].old_y, robots[x].radius, robots[x].radius, 0, 360*64);
			if(robots[x].death) {
				if(robots[x].death==20) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==18) {
					XFillArc(dpy, window, EXPLODE_COLOR_2, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==17) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==15) {
					XFillArc(dpy, window, black, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==14) {
					XFillArc(dpy, window, EXPLODE_COLOR_2, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==13) {
					XFillArc(dpy, window, black, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==12) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==11) {
					XFillArc(dpy, window, black, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==10) {
					XFillArc(dpy, window, EXPLODE_COLOR_2, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==9) {
					XFillArc(dpy, window, black, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==8) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==7) {
					XFillArc(dpy, window, black, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(robots[x].death==6) {
					XFillArc(dpy, window, EXPLODE_COLOR_2, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_2, EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(robots[x].death==4) {
					XFillArc(dpy, window, black, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_2, EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(robots[x].death==3) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_2, EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(robots[x].death==2) {
					XFillArc(dpy, window, black, robots[x].new_x+(robots[x].radius/3), robots[x].new_y+(robots[x].radius/3), EXPLODE_SIZE_2, EXPLODE_SIZE_2, 0, 360*64);
					XFillArc(dpy, window, EXPLODE_COLOR_2, robots[x].new_x+(1.7*robots[x].radius/2), robots[x].new_y+(1.7*robots[x].radius/2), EXPLODE_SIZE_3, EXPLODE_SIZE_3, 0, 360*64);
				}
				else if(robots[x].death==1) {
					XFillArc(dpy, window, black, robots[x].new_x+(1.7*robots[x].radius/2), robots[x].new_y+(1.7*robots[x].radius/2), EXPLODE_SIZE_3, EXPLODE_SIZE_3, 0, 360*64);
				}
				robots[x].death--;
			}
		}
	}

	for(x=0;x<NUM_ROBOTS;x++) {
		for(y=0;y<NUM_LASERS;y++) {
			if(robots[x].lasers[y].active) {
				XDrawLine(dpy, window, black, robots[x].lasers[y].start_x,
							 robots[x].lasers[y].start_y,
							 robots[x].lasers[y].end_x,
							 robots[x].lasers[y].end_y);
				move_laser(x, y);
				if(robots[x].lasers[y].active) {
					XDrawLine(dpy, window, robots[x].laser_color, robots[x].lasers[y].start_x,
								 robots[x].lasers[y].start_y,
								 robots[x].lasers[y].end_x,
								 robots[x].lasers[y].end_y);
				}
				else {
					XDrawLine(dpy, window, black, robots[x].lasers[y].start_x,
								 robots[x].lasers[y].start_y,
								 robots[x].lasers[y].end_x,
								 robots[x].lasers[y].end_y);
				}					
			}
		}
	}

	if(MOTHER_SHIP) {
		if(mother->active) {
			XFillArc(dpy, window, black, mother->old_x, mother->y, MOTHER_SHIP_WIDTH, MOTHER_SHIP_HEIGHT, 0 , 360*64);
			XFillArc(dpy, window, mother->ship_color, mother->new_x, mother->y, MOTHER_SHIP_WIDTH, MOTHER_SHIP_HEIGHT, 0 , 360*64);
		}
		else {
			if(mother->death) {
				XFillArc(dpy, window, black, mother->old_x, mother->y, MOTHER_SHIP_WIDTH, MOTHER_SHIP_HEIGHT, 0 , 360*64);
				if(mother->death==20) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==18) {
					XFillArc(dpy, window, EXPLODE_COLOR_2, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==17) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==15) {
					XFillArc(dpy, window, black, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==14) {
					XFillArc(dpy, window, EXPLODE_COLOR_2, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==13) {
					XFillArc(dpy, window, black, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==12) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==11) {
					XFillArc(dpy, window, black, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==10) {
					XFillArc(dpy, window, EXPLODE_COLOR_2, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==9) {
					XFillArc(dpy, window, black, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==8) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==7) {
					XFillArc(dpy, window, black, mother->new_x+1, mother->y+1, EXPLODE_SIZE_1, EXPLODE_SIZE_1, 0, 360*64);
				}
				else if(mother->death==6) {
					XFillArc(dpy, window, EXPLODE_COLOR_2, mother->new_x+1, mother->y+1, EXPLODE_SIZE_2, EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(mother->death==4) {
					XFillArc(dpy, window, black, mother->new_x+1, mother->y+1, EXPLODE_SIZE_2, EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(mother->death==3) {
					XFillArc(dpy, window, EXPLODE_COLOR_1, mother->new_x+1, mother->y+1, EXPLODE_SIZE_2, EXPLODE_SIZE_2, 0, 360*64);
				}
				else if(mother->death==2) {
					XFillArc(dpy, window, black, mother->new_x+1, mother->y+1, EXPLODE_SIZE_2, EXPLODE_SIZE_2, 0, 360*64);
					XFillArc(dpy, window, EXPLODE_COLOR_2, mother->new_x+(1.7*MOTHER_SHIP_WIDTH/2), mother->y+(1.7*MOTHER_SHIP_HEIGHT/2), EXPLODE_SIZE_3, EXPLODE_SIZE_3, 0, 360*64);
				}
				else if(mother->death==1) {
					XFillArc(dpy, window, black, mother->new_x+(1.7*MOTHER_SHIP_WIDTH/2), mother->y+(1.7*MOTHER_SHIP_HEIGHT/2), EXPLODE_SIZE_3, EXPLODE_SIZE_3, 0, 360*64);
				}
				mother->death--;
			}
		}
		for(y=0;y<8;y++) {
			if(mother->lasers[y].active) {
				XDrawLine(dpy, window, black, mother->lasers[y].start_x,
							 mother->lasers[y].start_y,
							 mother->lasers[y].end_x,
							 mother->lasers[y].end_y);
				move_laser(-1,y);
				if(mother->lasers[y].active) {
				XDrawLine(dpy, window, mother->laser_color, mother->lasers[y].start_x,
							 mother->lasers[y].start_y,
							 mother->lasers[y].end_x,
							 mother->lasers[y].end_y);
				}
				else {
					XDrawLine(dpy, window, black, mother->lasers[y].start_x,
								 mother->lasers[y].start_y,
								 mother->lasers[y].end_x,
								 mother->lasers[y].end_y);
				}
			}
		}
	}
}

/* This is the main loop. The mothership movement and laser firing happens inside
	this loop. */
static void 
start_blaster(void)
{
	int x=0;
	int y=0;
	int z=0;
	robots = (struct robot_state *) malloc(NUM_ROBOTS * sizeof (struct robot_state));
	for(x=0;x<NUM_ROBOTS;x++) {
		robots[x].alive = 0;
		robots[x].death = 0;
		robots[x].lasers = (struct laser_state *) malloc (NUM_LASERS * sizeof(struct laser_state));
		for(y=0;y<NUM_LASERS;y++) {
			robots[x].lasers[y].active = 0;
		}
	}

	if(NUM_STARS) {
		stars = (XArc *) malloc (NUM_STARS * sizeof(XArc));
		for(x=0;x<NUM_STARS;x++) {
			stars[x].x = random()%xgwa.width;
			stars[x].y = random()%xgwa.height;
			stars[x].width = random()%4 + 1;
			stars[x].height = stars[x].width;
			stars[x].angle1 = 0;
			stars[x].angle2 = 360 * 64;
		}
	}

	while(1) {
 		if(NUM_STARS) {
			if(MOVE_STARS) {
				XFillArcs(dpy,window,black,stars,NUM_STARS);
				if(MOVE_STARS_RANDOM) {
					y = MOVE_STARS_X;
					z = MOVE_STARS_Y;
					if(random()%167==0) {
						y = (-1)*y;
					}
					if(random()%173==0) {
						z = (-1)*z;
					}
					if(random()%50==0) {
						if(random()%2) {
							y++;
							if(y>MOVE_STARS_RANDOM) {
								y = MOVE_STARS_RANDOM;
							}
						}
						else {
							y--;
							if(y < -(MOVE_STARS_RANDOM)) {
								y = -(MOVE_STARS_RANDOM);
							}
						}
					}
					if(random()%50==0) {
						if(random()%2) {
							z++;
							if(z>MOVE_STARS_RANDOM) {
								z = MOVE_STARS_RANDOM;
							}
						}
						else {
							z--;
							if(z < -MOVE_STARS_RANDOM) {
								z = -MOVE_STARS_RANDOM;
							}
						}
					}
					MOVE_STARS_X = y;
					MOVE_STARS_Y = z;
					for(x=0;x<NUM_STARS;x++) {
						stars[x].x = stars[x].x + y;
						stars[x].y = stars[x].y + z;
						if(stars[x].x<0) {
							stars[x].x = stars[x].x + xgwa.width;
						}
						else if(stars[x].x>xgwa.width) {
							stars[x].x = stars[x].x - xgwa.width;
						}
						if(stars[x].y<0) {
							stars[x].y = stars[x].y + xgwa.height;
						}
						else if(stars[x].y>xgwa.height) {
							stars[x].y = stars[x].y - xgwa.height;
						}
					}
				}
				else {
					for(x=0;x<NUM_STARS;x++) {
						stars[x].x = stars[x].x + MOVE_STARS_X;
						stars[x].y = stars[x].y + MOVE_STARS_Y;
						if(stars[x].x<0) {
							stars[x].x = stars[x].x + xgwa.width;
						}
						else if(stars[x].x>xgwa.width) {
							stars[x].x = stars[x].x - xgwa.width;
						}
						if(stars[x].y<0) {
							stars[x].y = stars[x].y + xgwa.height;
						}
						else if(stars[x].y>xgwa.height) {
							stars[x].y = stars[x].y - xgwa.height;
						}
					}
				}
				XFillArcs(dpy,window,s_color,stars,NUM_STARS);
			}
			else {
				XFillArcs(dpy,window,s_color,stars,NUM_STARS);
			}
		}

		if(MOTHER_SHIP) {
			if(random()%MOTHER_SHIP_PERIOD==0) {
				if((mother->active==0)&&(mother->death==0)) {
					mother->active = MOTHER_SHIP_HITS;
					mother->y = random()%(xgwa.height-7);
					if(random()%2==0) {
						mother->old_x=0;
						mother->new_x=0;
					}
					else {
						mother->old_x=xgwa.width-25;
						mother->new_x=xgwa.width-25;
					}
				}
			}
		}
		move_robots();
		if(MOTHER_SHIP) {
			if(mother->active) {
				if(mother->old_x==mother->new_x) {
					if(mother->old_x==0) {
						mother->new_x=3;
					}
					else {
						mother->new_x=mother->new_x-3;
					}
				}
				else {
					if(mother->old_x>mother->new_x) {
						mother->old_x = mother->new_x;
						mother->new_x = mother->new_x-3;
						if(mother->new_x<0) {
							mother->active=0;
							XFillArc(dpy, window, black, mother->old_x, mother->y, MOTHER_SHIP_WIDTH, MOTHER_SHIP_HEIGHT, 0 , 360*64);
							XFillArc(dpy, window, black, mother->new_x, mother->y, MOTHER_SHIP_WIDTH, MOTHER_SHIP_HEIGHT, 0 , 360*64);
						}
					}
					else {
						mother->old_x = mother->new_x;
						mother->new_x = mother->new_x+3;
						if(mother->new_x>xgwa.width) {
							mother->active=0;
							XFillArc(dpy, window, black, mother->old_x, mother->y, MOTHER_SHIP_WIDTH, MOTHER_SHIP_HEIGHT, 0 , 360*64);
							XFillArc(dpy, window, black, mother->new_x, mother->y, MOTHER_SHIP_WIDTH, MOTHER_SHIP_HEIGHT, 0 , 360*64);
						}
					}
				}
				y=0;
				for(x=0;x<8;x++) {
					if(mother->lasers[x].active) {
						y=1;
						x=8;
					}
				}
				if(y==0) {
					for(x=0;x<8;x++) {
						mother->lasers[x].active = 1;
						mother->lasers[x].start_x=mother->new_x+(MOTHER_SHIP_WIDTH/2);
						mother->lasers[x].start_y=mother->y+(MOTHER_SHIP_HEIGHT/2);
					}
					y = (int)(MOTHER_SHIP_LASER/1.5);
					mother->lasers[0].end_x=mother->lasers[0].start_x-MOTHER_SHIP_LASER;
					mother->lasers[0].end_y=mother->lasers[0].start_y;
					mother->lasers[1].end_x=mother->lasers[1].start_x-y;
					mother->lasers[1].end_y=mother->lasers[1].start_y-y;
					mother->lasers[2].end_x=mother->lasers[2].start_x;
					mother->lasers[2].end_y=mother->lasers[2].start_y-MOTHER_SHIP_LASER;
					mother->lasers[3].end_x=mother->lasers[3].start_x+y;
					mother->lasers[3].end_y=mother->lasers[3].start_y-y;
					mother->lasers[4].end_x=mother->lasers[4].start_x+MOTHER_SHIP_LASER;
					mother->lasers[4].end_y=mother->lasers[4].start_y;
					mother->lasers[5].end_x=mother->lasers[5].start_x+y;
					mother->lasers[5].end_y=mother->lasers[5].start_y+y;
					mother->lasers[6].end_x=mother->lasers[6].start_x;
					mother->lasers[6].end_y=mother->lasers[6].start_y+MOTHER_SHIP_LASER;
					mother->lasers[7].end_x=mother->lasers[7].start_x-y;
					mother->lasers[7].end_y=mother->lasers[7].start_y+y;
				}
			}
		}
		draw_robots();

		XSync(dpy, False);
		screenhack_handle_events(dpy);
		if(delay) usleep(delay);
	}
}








char *progclass = "Blaster";

char *defaults [] = {
  ".background:	black",
  ".foreground:	white",
  "*r_color0:	magenta",
  "*r_color1:	orange",
  "*r_color2:	yellow",
  "*r_color3:	white",
  "*r_color4:	blue",
  "*r_color5:	cyan",
  "*l_color0:	green",
  "*l_color1:	red",
  "*mother_ship_color0:	darkblue",
  "*mother_ship_color1:	white",
  "*explode_color_1: yellow",
  "*explode_color_2: orange",
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

XrmOptionDescRec options [] = {
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



void screenhack(Display *d, Window w)
{
	XGCValues gcv;
	Colormap cmap;
	unsigned long bg;

	dpy = d;
	window = w;
	XGetWindowAttributes(dpy, window, &xgwa);
	cmap = xgwa.colormap;

	delay = get_integer_resource("delay", "Integer");
	if(delay==0) {
		delay=10000;
	}
	NUM_ROBOTS = get_integer_resource("num_robots","Integer");
	if(NUM_ROBOTS==0) {
		NUM_ROBOTS=5;
	}
	NUM_LASERS = get_integer_resource("num_lasers","Integer");
	EXPLODE_SIZE_1 = get_integer_resource("explode_size_1","Integer");
	EXPLODE_SIZE_2 = get_integer_resource("explode_size_2","Integer");
	EXPLODE_SIZE_3 = get_integer_resource("explode_size_3","Integer");

	NUM_STARS = get_integer_resource("num_stars","Integer");
	if(get_boolean_resource("move_stars","Boolean")) {
		MOVE_STARS = 1;
		MOVE_STARS_X = get_integer_resource("move_stars_x","Integer");
		MOVE_STARS_Y = get_integer_resource("move_stars_y","Integer");
		MOVE_STARS_RANDOM = get_integer_resource("move_stars_random","Integer");
	}
	else {
		MOVE_STARS = 0;
	}


	bg = get_pixel_resource("background","Background", dpy, cmap);
	gcv.function = GXcopy;

#define make_gc(color,name) \
	gcv.foreground = bg ^ get_pixel_resource ((name), "Foreground", \
						  dpy, cmap);		\
	color = XCreateGC (dpy, window, GCForeground|GCFunction, &gcv)

	if(mono_p) {
		gcv.foreground = bg;
		black = XCreateGC(dpy, window, GCForeground|GCFunction, &gcv);
		gcv.foreground = get_pixel_resource("foreground", "Foreground", dpy, cmap);
		r_color0 = r_color1 = r_color2 = r_color3 = r_color4 = r_color5 = l_color0 = l_color1 = s_color=
			XCreateGC(dpy, window, GCForeground|GCFunction, &gcv);
		if(get_boolean_resource("mother_ship","Boolean")) {
			MOTHER_SHIP_WIDTH=get_integer_resource("mother_ship_width","Integer");
			MOTHER_SHIP_HEIGHT=get_integer_resource("mother_ship_height","Integer");
			MOTHER_SHIP_LASER=get_integer_resource("mother_ship_laser","Integer");
			MOTHER_SHIP_PERIOD=get_integer_resource("mother_ship_period","Integer");
			MOTHER_SHIP_HITS=get_integer_resource("mother_ship_hits","Integer");
			MOTHER_SHIP=1;
			mother = (struct mother_ship_state *) malloc(sizeof(struct mother_ship_state));
			mother->lasers = (struct laser_state *) malloc(8*sizeof(struct laser_state));
			mother->active = 0;
			mother->death = 0;
			mother->ship_color = r_color0;
			mother->laser_color = r_color0;
		}
	}
	else {
		if(get_boolean_resource("mother_ship","Boolean")) {
			MOTHER_SHIP_WIDTH=get_integer_resource("mother_ship_width","Integer");
			MOTHER_SHIP_HEIGHT=get_integer_resource("mother_ship_height","Integer");
			MOTHER_SHIP_LASER=get_integer_resource("mother_ship_laser","Integer");
			MOTHER_SHIP_PERIOD=get_integer_resource("mother_ship_period","Integer");
			MOTHER_SHIP_HITS=get_integer_resource("mother_ship_hits","Integer");
			MOTHER_SHIP=1;
			mother = (struct mother_ship_state *) malloc(sizeof(struct mother_ship_state));
			mother->lasers = (struct laser_state *) malloc(8*sizeof(struct laser_state));
			mother->active = 0;
			mother->death = 0;
			make_gc(mother->ship_color,"mother_ship_color0");
			make_gc(mother->laser_color,"mother_ship_color1");		
		}

		make_gc (s_color,"star_color");
		
		make_gc (EXPLODE_COLOR_1,"explode_color_1");
		make_gc (EXPLODE_COLOR_2,"explode_color_2");
		
      make_gc (r_color0,"r_color0");
      make_gc (r_color1,"r_color1");
      make_gc (r_color2,"r_color2");
      make_gc (r_color3,"r_color3");
      make_gc (r_color4,"r_color4");
      make_gc (r_color5,"r_color5");
      make_gc (l_color0,"l_color0");
      make_gc (l_color1,"l_color1");
		make_gc (black,"background");
	}

	start_blaster();
}
