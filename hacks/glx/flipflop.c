/* flipflop, Copyright (c) 2003 Kevin Ogden <kogden1@hotmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#include <X11/Intrinsic.h>

#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#define BOARDSIZE 9
#define NUMSQUARES 76
#define HALFTHICK 0.04

#ifdef STANDALONE
# define PROGCLASS        "Flipflop"
# define HACK_INIT        init_flipflop
# define HACK_DRAW        draw_flipflop
# define HACK_RESHAPE     reshape_flipflop
# define HACK_HANDLE_EVENT flipflop_handle_event
# define EVENT_MASK       PointerMotionMask
# define flipflop_opts  xlockmore_opts

#define DEFAULTS       "*delay:       20000       \n" \
                       "*showFPS:       False       \n" \
		       "*wireframe:	False     \n"

# include "xlockmore.h"

#else
# include "xlock.h"
#endif

#ifdef USE_GL

#include <GL/glu.h>
#include "gltrackball.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static XrmOptionDescRec opts[] = {
  {"+rotate", ".flipflop.rotate", XrmoptionNoArg, "false" },
  {"-rotate", ".flipflop.rotate", XrmoptionNoArg, "true" },
};



static int rotate, wire, clearbits;

static argtype vars[] = {
  { &rotate, "rotate", "Rotate", "True", t_Bool},
};

ModeSpecOpt flipflop_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   flipflop_description =
{"flipflop", "init_flipflop", "draw_flipflop", "release_flipflop",
 "draw_flipflop", "init_flipflop", NULL, &flipflop_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Flipflop", 0, NULL};

#endif

typedef struct {
  GLXContext *glx_context;
  Window window;
  trackball_state *trackball;
  Bool button_down_p;
} Flipflopcreen;

static Flipflopcreen *qs = NULL;

typedef struct{
  /* 2D array specifying which squares are where (to avoid collisions) */
  /* -1 means empty otherwise integer represents square index 0 - n-1 */
  int occupied[ BOARDSIZE ][ BOARDSIZE ];
  /* an array of xpositions of the squares */
  int xpos[ NUMSQUARES ];
  /* array of y positions of the squares */
  int ypos[ NUMSQUARES ];
  /* integer representing the direction of movement of a square */
  int direction[ NUMSQUARES ]; /* 0 not, 1 x+, 2 y+, 3 x-, 4 y-*/
  /* angle of moving square (during a flip) */
  float angle[ NUMSQUARES ];
  /* array of colors for a square.  rgb */
  /* eg. color[ 4 ][ 0 ] is the red component of square 4 */
  /* eg. color[ 5 ][ 2 ] is the blue component of square 5  */
  float color[ NUMSQUARES ][ 3 ];
  /* n is the number of square */
} randsheet;


/*** ADDED RANDSHEET VARS ***/

static randsheet MyRandSheet;

static double theta = 0.0;
/* amount which the square flips.  1 is a entire flip */
static float flipspeed = 0.03;
/* relative distace of camera from center */
static float reldist = 1;
/* likelehood a square will attempt a move */
static float energy = 40;


static void randsheet_initialize( randsheet *rs );
static int randsheet_new_move( randsheet* rs );
static int randsheet_new_move( randsheet* rs );
static void randsheet_move( randsheet *rs, float rot );
static void randsheet_draw( randsheet *rs );
static void setup_lights(void);
static void drawBoard(void);
static void display(Flipflopcreen *c);
static void draw_sheet(void);


/* configure lighting */
static void
setup_lights(void)
{
/*   GLfloat position0[] = { BOARDSIZE*0.5, BOARDSIZE*0.1, BOARDSIZE*0.5, 1.0 }; */

/*   GLfloat position0[] = { -BOARDSIZE*0.5, 0.2*BOARDSIZE, -BOARDSIZE*0.5, 1.0 }; */
  GLfloat position0[] = { 0, BOARDSIZE*0.3, 0, 1.0 };

  if (wire) return;

  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, position0);
  glEnable(GL_LIGHT0);
 }

Bool
flipflop_handle_event (ModeInfo *mi, XEvent *event)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button & Button1)
    {
      c->button_down_p = True;
      gltrackball_start (c->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button & Button1)
    {
      c->button_down_p = False;
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           c->button_down_p)
    {
      gltrackball_track (c->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}

/* draw board */
static void
drawBoard(void)
{
  int i;
  for( i=0; i < (energy) ; i++ )
    randsheet_new_move( &MyRandSheet );
  randsheet_move( &MyRandSheet, flipspeed * 3.14159 );
  randsheet_draw( &MyRandSheet );
}


static void
display(Flipflopcreen *c)
{
   GLfloat amb[] = { 0.8, 0.8, 0.8, 1.0 };


 glClear(clearbits);
 glMatrixMode(GL_MODELVIEW);
 glLoadIdentity();

  glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.2);
  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.15/BOARDSIZE );
  glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.15/BOARDSIZE );
   glLightfv(GL_LIGHT0, GL_AMBIENT, amb);


  /** setup perspectif */
  glTranslatef(0.0, 0.0, -reldist*BOARDSIZE);
  glRotatef(22.5, 1.0, 0.0, 0.0);
  gltrackball_rotate (c->trackball);
  glRotatef(theta*100, 0.0, 1.0, 0.0);
  glTranslatef(-0.5*BOARDSIZE, 0.0, -0.5*BOARDSIZE);

  drawBoard();

  if (!c->button_down_p)
    theta += .001;

}

void
reshape_flipflop(ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  glViewport(0,0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1/h, 1.0, 300.0);
  glMatrixMode(GL_MODELVIEW);
}

void
init_flipflop(ModeInfo *mi)
{
  int screen = MI_SCREEN(mi);
  Flipflopcreen *c;
  wire = MI_IS_WIREFRAME(mi);

  if(!qs &&
     !(qs = (Flipflopcreen *) calloc(MI_NUM_SCREENS(mi), sizeof(Flipflopcreen))))
    return;

  c = &qs[screen];
  c->window = MI_WINDOW(mi);
  c->trackball = gltrackball_init ();

  if((c->glx_context = init_GL(mi)))
    reshape_flipflop(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  else
    MI_CLEARWINDOW(mi);

  glClearColor(0.0, 0.0, 0.0, 0.0);

  clearbits = GL_COLOR_BUFFER_BIT;

  glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);
  setup_lights();

  glEnable(GL_DEPTH_TEST);
  clearbits |= GL_DEPTH_BUFFER_BIT;
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  randsheet_initialize( &MyRandSheet );


}

void
draw_flipflop(ModeInfo *mi)
{
  Flipflopcreen *c = &qs[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if(!c->glx_context)
    return;

  glXMakeCurrent(disp, w, *(c->glx_context));

  display(c);

  if(mi->fps_p){
    do_fps(mi);
  }

  glFinish();
  glXSwapBuffers(disp, w);


}

void
release_flipflop(ModeInfo *mi)
{
  if(qs)
    free((void *) qs);

  FreeAllGL(MI);
}

/*** ADDED RANDSHEET FUNCTIONS ***/

static void
draw_sheet(void)
{
  glBegin( wire ? GL_LINE_LOOP : GL_QUADS );

  glNormal3f( 0, -1, 0 );
  glVertex3f( HALFTHICK,  -HALFTHICK,  HALFTHICK );
  glVertex3f( 1-HALFTHICK,   -HALFTHICK, HALFTHICK );
  glVertex3f( 1-HALFTHICK, -HALFTHICK,  1-HALFTHICK);
  glVertex3f( HALFTHICK, -HALFTHICK, 1-HALFTHICK );

  if (wire) { glEnd(); glBegin (GL_LINE_LOOP); }

  /* back */
  glNormal3f( 0, 1, 0 );
  glVertex3f( HALFTHICK, HALFTHICK, 1-HALFTHICK );
  glVertex3f( 1-HALFTHICK, HALFTHICK,  1-HALFTHICK);
  glVertex3f( 1-HALFTHICK,   HALFTHICK, HALFTHICK );
  glVertex3f( HALFTHICK,  HALFTHICK,  HALFTHICK );

  if (wire) { glEnd(); return; }

  /* 4 edges!!! weee.... */
  glNormal3f( 0, 0, -1 );
  glVertex3f( HALFTHICK, HALFTHICK, HALFTHICK );
  glVertex3f( 1-HALFTHICK, HALFTHICK, HALFTHICK );
  glVertex3f( 1-HALFTHICK, -HALFTHICK, HALFTHICK );
  glVertex3f( HALFTHICK, -HALFTHICK, HALFTHICK );
  glNormal3f( 0, 0, 1 );
  glVertex3f( HALFTHICK, HALFTHICK, 1-HALFTHICK );
  glVertex3f( HALFTHICK, -HALFTHICK, 1-HALFTHICK );
  glVertex3f( 1-HALFTHICK, -HALFTHICK, 1-HALFTHICK );
  glVertex3f( 1-HALFTHICK, HALFTHICK, 1-HALFTHICK );
  glNormal3f( 1, 0, 0 );
  glVertex3f( 1-HALFTHICK, HALFTHICK, 1-HALFTHICK );
  glVertex3f( 1-HALFTHICK, -HALFTHICK, 1-HALFTHICK );
  glVertex3f( 1-HALFTHICK, -HALFTHICK, HALFTHICK );
  glVertex3f( 1-HALFTHICK, HALFTHICK, HALFTHICK );
  glNormal3f( -1, 0, 0 );
  glVertex3f( HALFTHICK, HALFTHICK, 1-HALFTHICK );
  glVertex3f( HALFTHICK, HALFTHICK, HALFTHICK );
  glVertex3f( HALFTHICK, -HALFTHICK, HALFTHICK );
  glVertex3f( HALFTHICK, -HALFTHICK, 1-HALFTHICK );
  glEnd();
}

static void
randsheet_initialize( randsheet *rs )
{
  int i, j, index;
      index = 0;
      /* put the moving sheets on the board */
      for( i = 0; i < BOARDSIZE; i++ )
	{
	  for( j = 0; j < BOARDSIZE; j++ )
	    {
	      /* initially fill up a corner with the moving squares */
	      if( index < NUMSQUARES )
		{
		  rs->occupied[ i ][ j ] = index;
		  rs->xpos[ index ] = i;
		  rs->ypos[ index ] = j;
		  /* have the square colors start out as a pattern */
		  rs->color[ index ][ 0 ] = ((i+j)%3 == 0)||((i+j+1)%3 == 0);
		  rs->color[ index ][ 1 ] = ((i+j+1)%3 == 0);
		  rs->color[ index ][ 2 ] = ((i+j+2)%3 == 0);
		 ++index;
		}
		/* leave everything else empty*/
	      else
		{
		  rs->occupied[ i ][ j ] = -1;
		}
	    }
	}
	/* initially everything is at rest */
      for( i=0; i<NUMSQUARES; i++ )
	{
	  rs->direction[ i ] = 0;
	  rs->angle[ i ] = 0;
	}
}

/* Pick and random square and direction and try to move it. */
/* May not actually move anything, just attempt a random move. */
/* Returns true if move was sucessful. */
/* This could probably be implemented faster in a dequeue */
/* to avoid trying to move a square which is already moving */
/* but speed is most likely bottlenecked by rendering anyway... */
static int
randsheet_new_move( randsheet* rs )
{
      int i, j;
      int num, dir;
      /* pick a random square */
      num = random( ) % NUMSQUARES;
      i = rs->xpos[ num ];
      j = rs->ypos[ num ];
      /* pick a random direction */
      dir = ( random( )% 4 ) + 1;

      if( rs->direction[ num ] == 0 )
	{
	  switch( dir )
	    {
	    case 1:
	      /* move up in x */
	      if( ( i + 1 ) < BOARDSIZE )
		{
		  if( rs->occupied[ i + 1 ][ j ] == -1 )
		    {
		      rs->direction[ num ] = dir;
		      rs->occupied[ i + 1 ][ j ] = num;
		      rs->occupied[ i ][ j ] = -1;
		      return 1;
		    }
		}
	      return 0;
	      break;
	    case 2:
	      /* move up in y */
	      if( ( j + 1 ) < BOARDSIZE )
		{
		  if( rs->occupied[ i ][ j + 1 ] == -1 )
		    {
		      rs->direction[ num ] = dir;
		      rs->occupied[ i ][ j + 1 ] = num;
		      rs->occupied[ i ][ j ] = -1;
		      return 1;
		    }
		}
	      return 0;
	      break;
	    case 3:
	      /* move down in x */
	      if( ( i - 1 ) >= 0 )
		{
		  if( rs->occupied[ i - 1][ j ] == -1 )
		    {
		      rs->direction[ num ] = dir;
		      rs->occupied[ i - 1][ j ] = num;
		      rs->occupied[ i ][ j ] = -1;
		      return 1;
		    }
		}
	      return 0;
	      break;
	    case 4:
	      /* move down in y */
	      if( ( j - 1 ) >= 0 )
		{
		  if( rs->occupied[ i ][ j - 1 ] == -1 )
		    {
		      rs->direction[ num ] = dir;
		      rs->occupied[ i ][ j - 1 ] = num;
		      rs->occupied[ i ][ j ] = -1;
		      return 1;
		    }
		}
	      return 0;
	      break;
	    default:
	      break;
	    }
	}
      return 0;
}

/*   move a single frame.  */
/*   Pass in the angle in rads the square rotates in a frame. */
static void
randsheet_move( randsheet *rs, float rot )
{
      int i, j, index;
      for( index = 0 ; index < NUMSQUARES; index++ )
	{
	  i = rs->xpos[ index ];
	  j = rs->ypos[ index ];
	  switch( rs->direction[ index ] )
	    {
	    case 0:
	      /* not moving */
	      break;
	    case 1:
	    /* move up in x */
	      rs->angle[ index ] += rot;
	      /* check to see if we have finished moving */
	      if( rs->angle[ index ] >= M_PI  )
		{
		  rs->xpos[ index ] += 1;
		  rs->direction[ index ] = 0;
		  rs->angle[ index ] = 0;
		}
	      break;
	    case 2:
	    /* move up in y */
	      rs->angle[ index ] += rot;
	      /* check to see if we have finished moving */
	      if( rs->angle[ index ] >= M_PI  )
		{
		  rs->ypos[ index ] += 1;
		  rs->direction[ index ] = 0;
		  rs->angle[ index ] = 0;
		}
	      break;
	    case 3:
	    /* down in x */
	      rs->angle[ index ] += rot;
	      /* check to see if we have finished moving */
	      if( rs->angle[ index ] >= M_PI  )
		{
		  rs->xpos[ index ] -= 1;
		  rs->direction[ index ] = 0;
		  rs->angle[ index ] = 0;
		}
	      break;
	    case 4:
	    /* up in x */
	      rs->angle[ index ] += rot;
 	      /* check to see if we have finished moving */
	      if( rs->angle[ index ] >= M_PI  )
		{
		  rs->ypos[ index ] -= 1;
		  rs->direction[ index ] = 0;
		  rs->angle[ index ] = 0;
		}
	      break;
	    default:
	      break;
	    }
	}
}


    /* draw all the moving squares  */
static void
randsheet_draw( randsheet *rs )
{
      int i, j;
      int index;
      /* for all moving squares ... */
      for( index = 0; index < NUMSQUARES; index++ )
	{
	  /* set color */
	  glColor3f( rs->color[ index ][ 0 ],
		     rs->color[ index ][ 1 ],
		     rs->color[ index ][ 2 ] );
		     /* find x and y position */
	  i = rs->xpos[ index ];
	  j = rs->ypos[ index ];
	  glPushMatrix();
	  switch( rs->direction[ index ] )
	    {
	    case 0:

	    /* not moving */
	      /* front */
	      glTranslatef( i, 0, j );
	      break;
	    case 1:
	      glTranslatef( i+1, 0, j );
	      glRotatef( 180 - rs->angle[ index ]*180/M_PI, 0, 0, 1 );

	      break;
	    case 2:
	      glTranslatef( i, 0, j+1 );
	      glRotatef( 180 - rs->angle[ index ]*180/M_PI, -1, 0, 0 );

	      break;
	    case 3:
	      glTranslatef( i, 0, j );
	      glRotatef( rs->angle[ index ]*180/M_PI, 0, 0, 1 );
	      break;
	    case 4:
	      glTranslatef( i, 0, j );
	      glRotatef( rs->angle[ index ]*180/M_PI, -1, 0, 0 );
	      break;
	    default:
	      break;
	    }
	  draw_sheet();
	  glPopMatrix();

	}
}

/**** END RANDSHEET FUNCTIONS ***/

#endif
