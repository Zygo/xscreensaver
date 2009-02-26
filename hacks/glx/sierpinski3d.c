/* -*- Mode: C; tab-width: 4 -*- */
/* Sierpinski3D --- 3D sierpinski gasket */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)sierpinski3D.c	00.01 99/11/04 xlockmore";

#endif

/*-
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 * 1999: written by Tim Robinson <the_luggage@bigfoot.com>
 *       a 3-D representation of the Sierpinski gasket fractal.
 */

/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */

#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS					"Sierpinski3D"
# define HACK_INIT					init_gasket
# define HACK_DRAW					draw_gasket
# define gasket_opts				xlockmore_opts
# define DEFAULTS	"*count:		1       \n"			\
			"*cycles:		9999    \n"			\
			"*delay:		100     \n"			\
			"*wireframe:	False	\n"
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"			/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

ModeSpecOpt gasket_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   gasket_description =
{"gasket", "init_gasket", "draw_gasket", "release_gasket",
 "draw_gasket", "init_gasket", NULL, &gasket_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Shows GL's Sierpinski gasket", 0, NULL};

#endif

typedef struct{
  GLfloat x;
  GLfloat y;
  GLfloat z;
} GL_VECTOR;

typedef struct {
  GLfloat     view_rotx, view_roty, view_rotz;
  GLfloat     light_colour[4];/* = {6.0, 6.0, 6.0, 1.0}; */
  GLfloat     pos[3];/* = {0.0, 0.0, 0.0}; */
  GLfloat     xinc,yinc,zinc;
  GLfloat     angle;
  GLuint      gasket1;
  GLXContext *glx_context;
  Window      window;
#if 0
  Window      win;
#endif
} gasketstruct;

static gasketstruct *gasket = NULL;

#include <GL/glu.h>

/* static GLuint limit; */

static void
compile_gasket(ModeInfo *mi)
{
  int i,p;
  int points = MI_CYCLES(mi) ? MI_CYCLES(mi) : 9999;
  GL_VECTOR   vertex[5];

  /* define verticies */
  vertex[0].x =  0.5; 
  vertex[0].y = -(1.0/3.0)*sqrt((2.0/3.0));
  vertex[0].z = -sqrt(3.0)/6.0;

  vertex[1].x = -0.5; 
  vertex[1].y = -(1.0/3.0)*sqrt((2.0/3.0)); 
  vertex[1].z = -sqrt(3.0)/6.0; 

  vertex[2].x = 0.0; 
  vertex[2].y = (2.0/3.0)*sqrt((2.0/3.0));
  vertex[2].z = -sqrt(3.0)/6.0; 

  vertex[3].x = 0.0; 
  vertex[3].y = 0.0; 
  vertex[3].z = sqrt(3.0)/3.0; 

  vertex[4].x = 0.0;
  vertex[4].y = 0.0; 
  vertex[4].z = 0.0;
  
  glBegin(GL_POINTS);
  for( i = 0; i < points ; i++ )
  { 
    p = NRAND(4);
    vertex[4].x = ( vertex[4].x + vertex[p].x )/2.0;
    vertex[4].y = ( vertex[4].y + vertex[p].y )/2.0;
    vertex[4].z = ( vertex[4].z + vertex[p].z )/2.0;

    glVertex4f( vertex[4].x, vertex[4].y, vertex[4].z, 1.0 );
  }
  glEnd();
}

static void
draw(ModeInfo *mi)
{
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];
  
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glLightfv(GL_LIGHT0, GL_AMBIENT, gp->light_colour);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);

  glPushMatrix();
  glTranslatef( gp->pos[0], gp->pos[1], gp->pos[2] );  

  glPushMatrix();
  glRotatef(2*gp->angle, 1.0, 0.0, 0.0);
  glRotatef(3*gp->angle, 0.0, 1.0, 0.0);
  glRotatef(  gp->angle, 0.0, 0.0, 1.0);
  glScalef( 8.0, 8.0, 8.0 );
  glCallList(gp->gasket1);
  
  glPopMatrix();

  glPopMatrix();

}


/* new window size or exposure */
static void
reshape(int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport(0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective( 30.0, 1/h, 1.0, 100.0 );
  gluLookAt( 0.0, 0.0, 15.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40.0);
  
  /* The depth buffer will be cleared, if needed, before the
  * next frame.  Right now we just want to black the screen.
  */
  glClear(GL_COLOR_BUFFER_BIT);
}

static void
pinit(ModeInfo *mi)
{
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];

  gp->xinc = 0.1*(1.0*rand()/RAND_MAX);
  gp->yinc = 0.1*(1.0*rand()/RAND_MAX);
  gp->zinc = 0.1*(1.0*rand()/RAND_MAX);
  gp->light_colour[0] = 6.0;
  gp->light_colour[1] = 6.0;
  gp->light_colour[2] = 6.0;
  gp->light_colour[3] = 1.0;
  gp->pos[0] = 0.0;     
  gp->pos[1] = 0.0;
  gp->pos[2] = 0.0;    
  /* draw the gasket */
  gp->gasket1 = glGenLists(1);
  glNewList(gp->gasket1, GL_COMPILE);
    compile_gasket(mi);
  glEndList();
}

void
init_gasket(ModeInfo *mi)
{
  int           screen = MI_SCREEN(mi);
  gasketstruct *gp;

  if (gasket == NULL)
  {
    if ((gasket = (gasketstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (gasketstruct))) == NULL)
	return;
  }
  gp = &gasket[screen];

  gp->window = MI_WINDOW(mi);
  gp->view_rotx = NRAND(360);
  gp->view_roty = NRAND(360);
  gp->view_rotz = NRAND(360);
  gp->angle = NRAND(360)/90.0;

  if ((gp->glx_context = init_GL(mi)) != NULL)
  {
    reshape(MI_WIDTH(mi), MI_HEIGHT(mi));
    pinit(mi);
  }
  else
  {
    MI_CLEARWINDOW(mi);
  }
}

void
draw_gasket(ModeInfo * mi)
{
  gasketstruct *gp = &gasket[MI_SCREEN(mi)];
  Display      *display = MI_DISPLAY(mi);
  Window        window = MI_WINDOW(mi);
  int           angle_incr = 1;
  int           rot_incr = 1;/*MI_COUNT(mi) ? MI_COUNT(mi) : 1;*/

  if (!gp->glx_context) return;

  glDrawBuffer(GL_BACK);

  glXMakeCurrent(display, window, *(gp->glx_context));
  draw(mi);

  /* do the colour change & movement thing */
  gp->angle = (int) (gp->angle + angle_incr) % 360;
  gp->light_colour[0] = 3.0*SINF(gp->angle/20.0) + 4.0;
  gp->light_colour[1] = 3.0*SINF(gp->angle/30.0) + 4.0;
  gp->light_colour[2] = 3.0*SINF(gp->angle/60.0) + 4.0;
  if ( FABSF( gp->pos[0] ) > 9.0 ) gp->xinc = -1.0 * gp->xinc;
  if ( FABSF( gp->pos[1] ) > 7.0 ) gp->yinc = -1.0 * gp->yinc;
  if ( FABSF( gp->pos[2] ) >15.0 ) gp->zinc = -1.0 * gp->zinc;
  gp->pos[0] += gp->xinc;
  gp->pos[1] += gp->yinc;
  gp->pos[2] += gp->zinc;    
  gp->view_rotx = (int) (gp->view_rotx + rot_incr) % 360;
  gp->view_roty = (int) (gp->view_roty +(rot_incr/2.0)) % 360;
  gp->view_rotz = (int) (gp->view_rotz +(rot_incr/3.0)) % 360;

  glFinish();
  glXSwapBuffers(display, window);
}

void
release_gasket(ModeInfo * mi)
{
  if (gasket != NULL)
  {
    int         screen;

    for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
    {
      gasketstruct *gp = &gasket[screen];

      if (gp->glx_context)
      {
	/* Display lists MUST be freed while their glXContext is current. */
        glXMakeCurrent(MI_DISPLAY(mi), gp->window, *(gp->glx_context));

        if (glIsList(gp->gasket1)) glDeleteLists(gp->gasket1, 1);
      }
    }
    (void) free((void *) gasket);
    gasket = NULL;
  }
  FreeAllGL(mi);
}


/*********************************************************/

#endif
