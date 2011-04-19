/* xscreensaver, Copyright (c) 1992, 1995, 1996, 1997, 1998, 2004, 2006
 *  Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* wormhole:
 * Animation of moving through a wormhole. Based on my own code written
 * a few years ago.
 * author: Jon Rafkind <jon@rafkind.com>
 * date: 1/19/04
 */

#include <math.h>
#include "screenhack.h"

#ifndef debug
#define debug printf("File:%s Line:%d\n", __FILE__, __LINE__ );
#endif

typedef struct STAR{
	int x, y;
	int calc_x, calc_y;
	int Z;
	int center_x, center_y;
} star;

typedef struct STARLINE{
	star begin, end;
} starline;

/*
typedef struct RGBHANDLE{
	XColor mine;
	unsigned short rwant, gwant, bwant;
} RGBHandle;
*/

typedef struct COLORCHANGER{
	XColor * shade;
	int min, max; 
	int shade_use;
	int shade_max;
	int min_want;
	/* RGBHandle handle_begin, handle_end; */
} color_changer;

typedef struct WORMHOLE{
	int diameter;
	int diameter_change;
	int actualx, actualy;
	double virtualx, virtualy;
	double speed;
	int ang;
	int want_ang;
	int want_x, want_y;
	int max_Z;
	int addStar;
	int spiral;
	color_changer changer;
	starline ** stars;
	int num_stars; /* top of array we are using */
	XColor black;
	Pixmap work;
} wormhole;

struct state {
  Display *dpy;
  Window window;

  int SCREEN_X, SCREEN_Y;
  int z_speed;
  int make_stars;

  int delay;
  wormhole worm;
  GC gc;
  Colormap cmap;
};


/*inline*/ static int rnd( int q )
{

	return random() % q;

}

static int gang( int x1, int y1, int x2, int y2 )
{

	int tang = 0;
	if ( x1 == x2 ) {
		if ( y1 < y2 )
			tang = 90;
		else
			tang = 270;
	} else if ( y1 == y2 ) {

		if ( x1 < x2 )
			tang = 0;
		else
			tang = 180;

	} else {
          tang = (int)(0.5+atan2( -(y2-y1), x2 - x1 ) * 180.0 / M_PI );
        }

	while ( tang < 0 )
		tang += 360;
	return tang % 360;

}

static void blend_palette( XColor * pal, int max, XColor * sc, XColor * ec ) 
{

	int q;

	int sc_r = sc->red;
	int sc_g = sc->green;
	int sc_b = sc->blue;

	int ec_r = ec->red;
	int ec_g = ec->green;
	int ec_b = ec->blue;

        for ( q = 0; q < max; q++ ) {
                float j = (float)( q ) / (float)( max );
                int f_r = (int)( 0.5 + (float)( sc_r ) + (float)( ec_r-sc_r ) * j );
                int f_g = (int)( 0.5 + (float)( sc_g ) + (float)( ec_g-sc_g ) * j );
                int f_b = (int)( 0.5 + (float)( sc_b ) + (float)( ec_b-sc_b ) * j );
                /* pal[q] = makecol( f_r, f_g, f_b ); */
		pal[q].red = f_r;
		pal[q].blue = f_b;
		pal[q].green = f_g; 
	}

}


/*
static void initHandle( RGBHandle * handle )
{

	handle->mine.red = rnd( 65536 );
	handle->mine.green = rnd( 65536 );
	handle->mine.blue = rnd( 65536 );
	handle->rwant = rnd( 65536 );
	handle->gwant = rnd( 65536 );
	handle->bwant = rnd( 65536 );

}
*/

static void initXColor( XColor * color )
{
	color->red = rnd( 50000 ) + 10000;
	color->blue = rnd( 50000 ) + 10000;
	color->green = rnd( 50000 ) + 10000;
}

static void initColorChanger( struct state *st, color_changer * ch )
{

	int q;
	XColor old_color, new_color;

	ch->shade_max = 2048;
	ch->shade_use = 128;
	ch->min = 0;
	ch->max = ch->shade_use;
	ch->min_want = rnd( ch->shade_max - ch->shade_use );
	ch->shade = (XColor *)malloc( sizeof(XColor) * ch->shade_max );
	memset( ch->shade, 0, sizeof(XColor) * ch->shade_max );

	initXColor( &old_color );
	initXColor( &new_color );
	
	for ( q = 0; q < ch->shade_max; q += ch->shade_use ){
		blend_palette( ch->shade + q, ch->shade_use, &old_color, &new_color );
		old_color = new_color;

		initXColor( &new_color );
	}

	for ( q = 0; q < ch->shade_max; q++ )
		XAllocColor( st->dpy, st->cmap, &( ch->shade[q] ) );

	/*
	initHandle( &(ch->handle_begin) );
	initHandle( &(ch->handle_end) );
	ch->shade = (XColor *)malloc( sizeof(XColor) * MAX_COLORS );
	ch->max = MAX_COLORS;
	memset( ch->shade, 0, sizeof(XColor) * ch->max );

	blend_palette( ch->shade, ch->max, &(ch->handle_begin.mine), &(ch->handle_end.mine) );
	for ( q = 0; q < ch->max; q++ )
		XAllocColor( st->dpy, *cmap, &( ch->shade[q] ) );
	*/

}

/*
static void changeColor( unsigned short * col, unsigned short * change, int min, int max )
{
	int RGB_GO_BLACK = 30;
	if ( *col < *change ) *col++;
	if ( *col > *change ) *col--;
	if ( *col == *change ){
		if ( rnd( RGB_GO_BLACK ) == rnd( RGB_GO_BLACK ) )
			*change = 0;
		else	*change = rnd(max-min) + min;
	}
}
*/

/*
static void moveRGBHandle( RGBHandle * handle, int min, int max )
{

	unsigned short * want[ 3 ];
	int q;
	int cy = 0;
	want[0] = &(handle->rwant);
	want[1] = &(handle->gwant);
	want[2] = &(handle->bwant);

	for ( q = 0; q < 10; q++ ){
		changeColor( &( handle->mine.red ), &handle->rwant, min, max );
		changeColor( &( handle->mine.green ), &handle->gwant, min, max );
		changeColor( &( handle->mine.blue ), &handle->bwant, min, max );
	}
	
	for ( q = 0; q < 3; q++ )
		cy = cy || (*(want[q]) >= min && *(want[q]) <= max);
	if ( !cy ) *(want[rnd(3)]) = rnd(max-min)+min;
	cy = 1;
	for ( q = 0; q < 3; q++ )
		cy = cy && *(want[q]) == 0;
	if ( cy ) *(want[rnd(3)]) = rnd(max-min)+min;

	if ( rnd( 30 ) == rnd( 30 ) )
		*(want[rnd(3)]) = rnd(max-min)+min;

}
*/

static void moveColorChanger( color_changer * ch )
{

	/* int q; */

	if ( ch->min < ch->min_want ){
		ch->min++;
		ch->max++;
	}
	if ( ch->min > ch->min_want ) {
		ch->min--;
		ch->max--;
	}
	if ( ch->min == ch->min_want )
		ch->min_want = rnd( ch->shade_max - ch->shade_use );

	/*
	for ( q = 0; q < ch->max; q++ )
		XFreeColors( st->dpy, *cmap, &( ch->shade[q].pixel ), 1, 0 );

	moveRGBHandle( &( ch->handle_begin ), 5000, 65500 );
	moveRGBHandle( &( ch->handle_end ), 5000, 65500 );
	
	blend_palette( ch->shade, ch->max, &(ch->handle_begin.mine), &(ch->handle_end.mine) );
	for ( q = 0; q < ch->max; q++ )
		XAllocColor( st->dpy, *cmap, &( ch->shade[q] ) );
	*/

}

#if 0
static void destroyColorChanger( color_changer * ch )
{
	int q;
	for ( q = 0; q < ch->max; q++ )
		XFreeColors( st->dpy, *cmap, &( ch->shade[q].pixel ), 1, 0 );
	free( ch->shade );
}
#endif

static void resizeWormhole( struct state *st, wormhole * worm )
{
	
	XWindowAttributes attr;

	XGetWindowAttributes( st->dpy, st->window, &attr );

	st->cmap = attr.colormap;
	
	st->SCREEN_X = attr.width;
	st->SCREEN_Y = attr.height;

# ifndef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
	XFreePixmap( st->dpy, worm->work );
	worm->work = XCreatePixmap( st->dpy, st->window, st->SCREEN_X, st->SCREEN_Y, attr.depth );
# endif

}

static void initWormhole( struct state *st, wormhole * worm, Display * display, Window win )
{
	
	int i;
	XWindowAttributes attr;

	XGetWindowAttributes( st->dpy, st->window, &attr );

	st->cmap = attr.colormap;
	
	st->SCREEN_X = attr.width;
	st->SCREEN_Y = attr.height;

# ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
        worm->work = st->window;
# else
	worm->work = XCreatePixmap( st->dpy, st->window, st->SCREEN_X, st->SCREEN_Y, attr.depth );
# endif

	worm->diameter = rnd( 10 ) + 15;
	worm->diameter_change = rnd( 10 ) + 15;
	/* worm->actualx = rnd( attr.width );
	worm->actualy = rnd( attr.height ); */
	worm->actualx = attr.width / 2;
	worm->actualy = attr.height / 2;
	worm->virtualx = worm->actualx;
	worm->virtualy = worm->actualy;
	worm->speed = (float)st->SCREEN_X / 180.0;
	/* z_speed = SCREEN_X / 120; */
	worm->spiral = 0;
	worm->addStar = st->make_stars;
	worm->want_x = rnd( attr.width - 50 ) + 25;
	worm->want_y = rnd( attr.height - 50 ) + 25;
	worm->want_ang = gang( worm->actualx, worm->actualy, worm->want_x, worm->want_y );
	worm->ang = worm->want_ang;
	worm->max_Z = 600;
	worm->black.red = 0;
	worm->black.green = 0;
	worm->black.blue = 0;
	XAllocColor( st->dpy, st->cmap, &worm->black );
	initColorChanger( st, &(worm->changer) );

	worm->num_stars = 64;
	worm->stars = (starline **)malloc( sizeof(starline *) * worm->num_stars );
	for ( i = 0; i < worm->num_stars; i++ )
		worm->stars[i] = NULL;

}

#if 0
static void destroyWormhole( wormhole * worm )
{
	destroyColorChanger( &(worm->changer), st->dpy, cmap );
        if (work->work != st->window)
          XFreePixmap( st->dpy, worm->work );
	free( worm->stars );
}
#endif

static double Cos( int a )
{
	return cos( a * 180.0 / M_PI );
}

static double Sine( int a )
{
	return sin( a * 180.0 / M_PI );
}



static void calcStar( star * st )
{
	if ( st->center_x == 0 || st->center_y == 0 ){
		st->Z = 0;
		return;
	}
	if ( st->Z <= 0 ){
		st->calc_x = (st->x << 10) / st->center_x;
		st->calc_y = (st->y << 10) / st->center_y;
	} else {
		st->calc_x = (st->x << 10 ) / st->Z + st->center_x;
		st->calc_y = (st->y << 10 ) / st->Z + st->center_y;
	}
}

static void initStar( star * st, int Z, int ang, wormhole * worm )
{

	st->x = Cos( ang ) * worm->diameter;
	st->y = Sine( ang ) * worm->diameter;
	st->center_x = worm->actualx;
	st->center_y = worm->actualy;
	st->Z = Z;
	calcStar( st );

}

static void addStar( wormhole * worm )
{

	starline * star_new;
	starline ** xstars;
	int old_stars;
	int q;
	int ang;
	star_new = (starline *)malloc( sizeof( starline ) );
	ang = rnd( 360 );
	initStar( &star_new->begin, worm->max_Z, ang, worm );
	initStar( &star_new->end, worm->max_Z+rnd(6)+4, ang, worm );

	for ( q = 0; q < worm->num_stars; q++ ){
		if ( worm->stars[q] == NULL ){
			worm->stars[q] = star_new;
			return;
		}
	}

	old_stars = worm->num_stars;
	worm->num_stars = worm->num_stars << 1;
	xstars = (starline **)malloc( sizeof(starline *) * worm->num_stars );
	for ( q = 0; q < worm->num_stars; q++ )
		xstars[q] = NULL;

	for ( q = 0; q < old_stars; q++ )
		if ( worm->stars[q] != NULL ) xstars[q] = worm->stars[q];
	free( worm->stars );
	worm->stars = xstars;

	worm->stars[ old_stars ] = star_new;

}

static int moveStar( struct state *st, starline * stl )
{

	stl->begin.Z -= st->z_speed;	
	stl->end.Z -= st->z_speed;

	calcStar( &stl->begin );
	calcStar( &stl->end );

	return ( stl->begin.Z <= 0 || stl->end.Z <= 0 );

} 

static int dist( int x1, int y1, int x2, int y2 )
{
	int xs, ys;
	xs = x1-x2;
	ys = y1-y2;
	return (int)sqrt( xs*xs + ys*ys );
}

static void moveWormhole( struct state *st, wormhole * worm )
{

	int q;
	double dx, dy;
	/* int x1, y1, x2, y2; */
	int min_dist = 100;
	int find = 0;
	dx = Cos( worm->ang ) * worm->speed;
	dy = Sine( worm->ang ) * worm->speed;

	worm->virtualx += dx;
	worm->virtualy += dy;
	worm->actualx = (int)worm->virtualx;
	worm->actualy = (int)worm->virtualy;

	if ( worm->spiral ){

		if ( worm->spiral % 5 == 0 )
			worm->ang = (worm->ang + 1 ) % 360;
		worm->spiral--;
		if ( worm->spiral <= 0 ) find = 1;

	} else {

		if ( dist( worm->actualx, worm->actualy, worm->want_x, worm->want_y ) < 20 )
			find = 1;

		if ( rnd( 20 ) == rnd( 20 ) )
			find = 1;

		if ( worm->actualx < min_dist ){
			worm->actualx = min_dist;
			worm->virtualx = worm->actualx;
			find = 1;
		}
		if ( worm->actualy < min_dist ){
			worm->actualy = min_dist;
			worm->virtualy = worm->actualy;
			find = 1;
		}
		if ( worm->actualx > st->SCREEN_X - min_dist ){
			worm->actualx = st->SCREEN_X - min_dist;
			worm->virtualx = worm->actualx;
			find = 1;
		}
		if ( worm->actualy > st->SCREEN_Y - min_dist ){
			worm->actualy = st->SCREEN_Y - min_dist;
			worm->virtualy = worm->actualy;
			find = 1;
		}
	
		if ( rnd( 500 ) == rnd( 500 ) ) worm->spiral = rnd( 30 ) + 50;
	}

	if ( find ){
		worm->want_x = rnd( st->SCREEN_X - min_dist * 2 ) + min_dist;
		worm->want_y = rnd( st->SCREEN_Y - min_dist * 2 ) + min_dist;
		worm->ang = gang( worm->actualx, worm->actualy, worm->want_x, worm->want_y );
	}


	/* worm->ang = ( worm->ang + 360 + rnd( 30 ) - 15 ) % 360; */

	/*
	if ( worm->ang < worm->want_ang ) worm->ang++;
	if ( worm->ang > worm->want_ang ) worm->ang--;
	if ( worm->ang == worm->want_ang && rnd( 3 ) == rnd( 3 ) ) worm->want_ang = rnd( 360 );
	*/

	/*
	if ( rnd( 25 ) == rnd( 25 ) ){
		x1 = worm->actualx;
		y1 = worm->actualy;
		x2 = SCREEN_X / 2 + rnd( 20 ) - 10;
		y2 = SCREEN_Y / 2 + rnd( 20 ) - 10;
		worm->want_ang = gang(x1,y1,x2,y2);
	}
	*/

	/*
	if ( worm->actualx < min_dist || worm->actualx > SCREEN_X - min_dist || worm->actualy < min_dist || worm->actualy > SCREEN_Y - min_dist ){
		x1 = worm->actualx;
		y1 = worm->actualy;
		x2 = SCREEN_X / 2 + rnd( 20 ) - 10;
		y2 = SCREEN_Y / 2 + rnd( 20 ) - 10;
		/ * worm->ang = gang( worm->actualx, worm->actualy, SCREEN_X/2+rnd(20)-10, SCREEN_Y/2+rnd(20)-10 ); * /
		worm->ang = gang( x1, y1, x2, y2 );
		worm->want_ang = worm->ang;
		/ * printf("Angle = %d\n", worm->ang ); * /

		if ( worm->actualx < min_dist )
			worm->actualx = min_dist;
		if ( worm->actualx > SCREEN_X - min_dist )
			worm->actualx = SCREEN_X - min_dist;
		if ( worm->actualy < min_dist )
			worm->actualy = min_dist;
		if ( worm->actualy > SCREEN_Y - min_dist )
			worm->actualy = SCREEN_Y - min_dist;
		worm->virtualx = worm->actualx;
		worm->virtualy = worm->actualy;
	}
	*/

	for ( q = 0; q < worm->num_stars; q++ ){
		if ( worm->stars[q] != NULL ){
			if ( moveStar( st, worm->stars[q] ) ){
				free( worm->stars[q] );
				worm->stars[q] = NULL;
			}
		}
	}

	moveColorChanger( &worm->changer );

	if ( worm->diameter < worm->diameter_change )
		worm->diameter++;
	if ( worm->diameter > worm->diameter_change )
		worm->diameter--;
	if ( rnd( 30 ) == rnd( 30 ) )
		worm->diameter_change = rnd( 35 ) + 5;

	for ( q = 0; q < worm->addStar; q++ )
		addStar( worm );

}

static XColor * getColorShade( color_changer * ch )
{
	return ch->shade + ch->min;
}

static void drawWormhole( struct state *st, wormhole * worm )
{

	int i;
	int color;
	int z;
	starline * current;
	XColor * xcol;
	XColor * shade;

	XSetForeground( st->dpy, st->gc, worm->black.pixel );
	XFillRectangle( st->dpy, worm->work, st->gc, 0, 0, st->SCREEN_X, st->SCREEN_Y );

	for ( i = 0; i < worm->num_stars; i++ )
		if ( worm->stars[i] != NULL ){
	
			current = worm->stars[i];
			z = current->begin.Z;

			color = z * worm->changer.shade_use / worm->max_Z;
			shade = getColorShade( &worm->changer );
			/* xcol = &worm->changer.shade[ color ]; */
			xcol = &shade[ color ];

			XSetForeground( st->dpy, st->gc, xcol->pixel );
			/* XDrawLine( st->dpy, st->window, *gc, current->begin.calc_x, current->begin.calc_y, current->end.calc_x, current->end.calc_y ); */
			XDrawLine( st->dpy, worm->work, st->gc, current->begin.calc_x, current->begin.calc_y, current->end.calc_x, current->end.calc_y );

		}
        if (worm->work != st->window)
          XCopyArea( st->dpy, worm->work, st->window, st->gc, 0, 0, st->SCREEN_X, st->SCREEN_Y, 0, 0 );
}

/*
static void eraseWormhole( Display * display, Window * st->window, wormhole * worm ){
	starline * current;
	int i;
	XColor * xcol;
	for ( i = 0; i < worm->num_stars; i++ )
		if ( worm->stars[i] != NULL ){
			xcol = &worm->black;
			current = worm->stars[i];
			XSetForeground( st->dpy, *gc, xcol->pixel );
			XDrawLine( st->dpy, st->window, *gc, current->begin.calc_x, current->begin.calc_y, current->end.calc_x, current->end.calc_y );
		}
}
*/



static void *
wormhole_init (Display *dpy, Window window)
{
	struct state *st = (struct state *) calloc (1, sizeof(*st));
  	XGCValues gcv;
	XWindowAttributes attr;

	st->dpy = dpy;
	st->window = window;
	st->delay = get_integer_resource(st->dpy,  "delay", "Integer" );
	st->make_stars = get_integer_resource(st->dpy,  "stars", "Integer" );
	st->z_speed = get_integer_resource(st->dpy,  "zspeed", "Integer" );

	initWormhole( st, &st->worm, st->dpy, st->window );

	st->gc = XCreateGC( st->dpy, st->window, 0, &gcv );
	XGetWindowAttributes( st->dpy, st->window, &attr );
	st->cmap = attr.colormap;

        return st;
}

static unsigned long
wormhole_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  moveWormhole( st, &st->worm );
  drawWormhole( st, &st->worm );
  return st->delay;
}

static void
wormhole_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  resizeWormhole( st, &st->worm );
}

static Bool
wormhole_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
wormhole_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}




static const char *wormhole_defaults [] = {
  ".background:	Black",
  ".foreground:	#E9967A",
  "*delay:	10000",
  "*zspeed:	10",
  "*stars:	20",
  0
};

static XrmOptionDescRec wormhole_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-zspeed",		".zspeed",	XrmoptionSepArg, 0 },
  { "-stars",		".stars",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Wormhole", wormhole)
