/*
 * queens - solves n queens problem, displays
 * i make no claims that this is an optimal solution to the problem,
 * good enough for xss
 * hacked from glchess
 *
 * version 1.0 - May 10, 2002
 *
 * Copyright (C) 2002 Blair Tennessy (tennessb@unbc.ca)
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

#ifdef STANDALONE
# define PROGCLASS        "Queens"
# define HACK_INIT        init_queens
# define HACK_DRAW        draw_queens
# define HACK_RESHAPE     reshape_queens
# define HACK_HANDLE_EVENT queens_handle_event
# define EVENT_MASK       PointerMotionMask
# define queens_opts  xlockmore_opts

#define DEFAULTS       "*delay:       20000       \n" \
                       "*showFPS:       False       \n" \
		       "*wireframe:	False     \n"	\

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
  {"+rotate", ".queens.rotate", XrmoptionNoArg, (caddr_t) "false" },
  {"-rotate", ".queens.rotate", XrmoptionNoArg, (caddr_t) "true" },
};

int rotate;

static argtype vars[] = {
  {(caddr_t *) &rotate, "rotate", "Rotate", "True", t_Bool},
};

ModeSpecOpt queens_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   queens_description =
{"queens", "init_queens", "draw_queens", "release_queens",
 "draw_queens", "init_queens", NULL, &queens_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Queens", 0, NULL};

#endif

typedef struct {
  GLXContext *glx_context;
  Window window;
  trackball_state *trackball;
  Bool button_down_p;
} Queenscreen;

static Queenscreen *qs = NULL;

#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif

#define NONE 0
#define QUEEN 1
#define MINBOARD 5
#define MAXBOARD 10

/* definition of white/black colors */
GLfloat colors[2][3] = { {0.5, 0.7, 0.9},
			 {0.2, 0.3, 0.6} };

int board[MAXBOARD][MAXBOARD];
int work = 0, vb = 0, steps = 0, BOARDSIZE = 8; /* 8 cuz its classic */

Bool
queens_handle_event (ModeInfo *mi, XEvent *event)
{
  Queenscreen *c = &qs[MI_SCREEN(mi)];

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



/* returns true if placing a queen on column c causes a conflict */
int conflictsCols(int c) {
  int i;

  for(i = 0; i < BOARDSIZE; ++i)
    if(board[i][c])
      return 1;

  return 0;
}

/* returns true if placing a queen on (r,c) causes a diagonal conflict */
int conflictsDiag(int r, int c) {
  int i;

  /* positive slope */
  int n = r < c ? r : c;
  for(i = 0; i < BOARDSIZE-abs(r-c); ++i)
    if(board[r-n+i][c-n+i])
      return 1;

  /* negative slope */
  n = r < BOARDSIZE - (c+1) ? r : BOARDSIZE - (c+1);
  for(i = 0; i < BOARDSIZE-abs(BOARDSIZE - (1+r+c)); ++i)
    if(board[r-n+i][c+n-i])
      return 1;
  
  return 0;
}

/* returns true if placing a queen at (r,c) causes a conflict */
int conflicts(int r, int c) {
  return !conflictsCols(c) ? conflictsDiag(r, c) : 1;
}

/* clear board */
void blank(void) {
  int i, j;

  for(i = 0; i < MAXBOARD; ++i)
    for(j = 0; j < MAXBOARD; ++j)
      board[i][j] = NONE;
}

/* recursively determine solution */
int findSolution(int row) {
  int col = 0;

  if(row == BOARDSIZE)
    return 1;
  
  while(col < BOARDSIZE) {
    if(!conflicts(row, col)) {
      board[row][col] = QUEEN;

      if(findSolution(row+1))
	return 1;

      board[row][col] = NONE; 
    }

    ++col;
  }

  return 0;
}

/* driver for finding solution */
void go(void) { findSolution(0); }

/* configure lighting */
void setup_lights(void) {
  GLfloat position[] = { 4.0, 8.0, 4.0, 1.0 };

  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, position);
  glEnable(GL_LIGHT0);
}

/* return alpha value for fading */
GLfloat findAlpha(void) {
  return steps < 128 ? steps/128.0 : steps < 512-128 ? 1.0 : (512-steps)/128.0;
}

/* draw pieces */
void drawPieces(void) {
  int i, j;

  for(i = 0; i < BOARDSIZE; ++i) {
    for(j = 0; j < BOARDSIZE; ++j) {
      if(board[i][j]) {
	glColor4f(colors[i%2][0], colors[i%2][1], colors[i%2][2], findAlpha());
	glCallList(QUEEN);
      }
      
      glTranslatef(1.0, 0.0, 0.0);
    }
    
    glTranslatef(-1.0*BOARDSIZE, 0.0, 1.0);
  }
}

/* draw board */
void drawBoard(int wire) {
  int i, j;

  if (!wire) glBegin(GL_QUADS);

  for(i = 0; i < BOARDSIZE; ++i)
    for(j = 0; j < BOARDSIZE; ++j) {
      int par = (i-j+BOARDSIZE)%2;
      glColor4f(colors[par][0], colors[par][1], colors[par][2], findAlpha());
      glNormal3f(0.0, 1.0, 0.0);
      if (wire) glBegin(GL_LINE_LOOP);
      glVertex3f(j - 0.5, -0.01, i - 0.5);
      glVertex3f(j + 0.5, -0.01, i - 0.5);
      glVertex3f(j + 0.5, -0.01, i + 0.5);
      glVertex3f(j - 0.5, -0.01, i + 0.5);
      if (wire) glEnd();
    }

  if (!wire) glEnd();
}

double theta = 0.0;

void display(Queenscreen *c, int wire) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(0.0, 1.0+(0.8*fabs(sin(theta)))*10.0, -1.2*BOARDSIZE,
 	    0.0, 1.0, 0.0,
 	    0.0, 0.0, 1.0);

  glScalef(1, -1, 1);
  gltrackball_rotate (c->trackball);	/* Apply mouse-based camera position */
  glScalef(1, -1, 1);

  glRotatef(theta*100, 0.0, 1.0, 0.0);
  glTranslatef(-0.5 * (BOARDSIZE-1), 0.0, -0.5 * (BOARDSIZE-1));

  if (!wire) {
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_BLEND);
  }

  drawBoard(wire);
  glTranslatef(0.0, 0.01, 0.0);  
  drawPieces();

  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_BLEND);
  glDisable(GL_LIGHTING);

  theta += .002;

  /* zero out board, find new solution of size MINBOARD <= i <= MAXBOARD */
  if(++steps == 512) {
    steps = 0;
    blank();
    BOARDSIZE = MINBOARD + (random() % (MAXBOARD - MINBOARD + 1));
    go();
  }
}

#define piece_size 0.1
#define EPSILON 0.001

/* Make a revolved piece */
void revolve_line(double *trace_r, double *trace_h, double max_iheight, 
		  int rot, int wire) {
  double theta, norm_theta, sin_theta, cos_theta;
  double norm_ptheta = 0.0, sin_ptheta = 0.0, cos_ptheta = 1.0;
  double radius, pradius;
  double max_height = max_iheight, height, pheight;
  double dx, dy, len;
  int npoints, p;
  double dtheta = (2.0*M_PI) / rot;

  /* Get the number of points */
  for(npoints = 0; 
      fabs(trace_r[npoints]) > EPSILON || fabs(trace_h[npoints]) > EPSILON;
      ++npoints);

  /* If less than two points, can not revolve */
  if(npoints < 2)
    return;

  /* If the max_height hasn't been defined, find it */
  if(max_height < EPSILON)
    for(p = 0; p < npoints; ++p)
      if(max_height < trace_h[p])
	max_height = trace_h[p];

  /* Draw the revolution */
  for(theta = dtheta; rot > 0; --rot) {
    sin_theta = sin(theta);
    cos_theta = cos(theta);
    norm_theta = theta / (2.0 * M_PI);
    pradius = trace_r[0] * piece_size;
    pheight = trace_h[0] * piece_size;
    
    for(p = 0; p < npoints; ++p) {
      radius = trace_r[p] * piece_size;
      height = trace_h[p] * piece_size;

      /* Get the normalized lengths of the normal vector */
      dx = radius - pradius;
      dy = height - pheight;
      len = sqrt(dx*dx + dy*dy);
      dx /= len;
      dy /= len;

      /* If only triangles required */
      if (fabs(radius) < EPSILON) {
 	glBegin(wire ? GL_LINE_LOOP : GL_TRIANGLES);

	glNormal3f(dy * sin_ptheta, -dx, dy * cos_ptheta);
	glTexCoord2f(norm_ptheta, pheight / max_height);
	glVertex3f(pradius * sin_ptheta, pheight, pradius * cos_ptheta);
	
	glNormal3f(dy * sin_theta, -dx, dy * cos_theta);
	glTexCoord2f(norm_theta, pheight / max_height);
	glVertex3f(pradius * sin_theta, pheight, pradius * cos_theta);
	
	glTexCoord2f(0.5 * (norm_theta + norm_ptheta),
		     height / max_height);
	glVertex3f(0.0, height, 0.0);
	
	glEnd();
      } 
      
      else {
	glBegin(wire ? GL_LINE_LOOP : GL_QUADS);

	glNormal3f(dy * sin_ptheta, -dx, dy * cos_ptheta);
	glTexCoord2f(norm_ptheta, pheight / max_height);
	glVertex3f(pradius * sin_ptheta, pheight, pradius * cos_ptheta);

	glNormal3f(dy * sin_theta, -dx, dy * cos_theta);
	glTexCoord2f(norm_theta, pheight / max_height);
	glVertex3f(pradius * sin_theta, pheight, pradius * cos_theta);

	glTexCoord2f(norm_theta, height / max_height);
	glVertex3f(radius * sin_theta, height, radius * cos_theta);

	glNormal3f(dy * sin_ptheta, -dx, dy * cos_ptheta);
	glTexCoord2f(norm_ptheta, height / max_height);
	glVertex3f(radius * sin_ptheta, height, radius * cos_ptheta);

	glEnd();
      }

      pradius = radius;
      pheight = height;
    }

    sin_ptheta = sin_theta;
    cos_ptheta = cos_theta;
    norm_ptheta = norm_theta;
    theta += dtheta;
  }
}

void draw_queen(int wire) {
  double trace_r[] =
      { 4.8, 4.8, 3.4, 3.4, 1.8, 1.4, 2.9, 1.8, 1.8, 2.0, 
	2.7, 2.4, 1.7, 0.95, 0.7, 0.0, 0.0 }; /*, 0.9, 0.7, 0.0, 0.0};*/
  double trace_h[] =
      { 0.0, 2.2, 4.0, 5.0, 8.0, 11.8, 11.8, 13.6, 15.2, 17.8,
	19.2, 20.0, 20.0, 20.8, 20.8, 22.0, 0.0 };/*,21.4, 22.0, 22.0, 0.0 };*/

  revolve_line(trace_r, trace_h, 0.0, 8, wire);
}

void reshape_queens(ModeInfo *mi, int width, int height) {
  GLfloat h = (GLfloat) height / (GLfloat) width;
  glViewport(0,0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1/h, 2.0, 30.0);
  glMatrixMode(GL_MODELVIEW);
}

void init_queens(ModeInfo *mi) {
  GLfloat mat_shininess[] = { 90.0 };
  GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };

  int screen = MI_SCREEN(mi);
  int wire = MI_IS_WIREFRAME(mi);
  Queenscreen *c;

  if(!qs && 
     !(qs = (Queenscreen *) calloc(MI_NUM_SCREENS(mi), sizeof(Queenscreen))))
    return;
  
  c = &qs[screen];
  c->window = MI_WINDOW(mi);
  c->trackball = gltrackball_init ();
  
  if((c->glx_context = init_GL(mi)))
    reshape_queens(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  else
    MI_CLEARWINDOW(mi);

  glClearColor(0.0, 0.0, 0.0, 0.0);

  setup_lights();
  glNewList(1, GL_COMPILE);
  draw_queen(wire);
  glEndList();

  if (!wire) {
    glColorMaterial(GL_FRONT, GL_DIFFUSE);

    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
  }

  /* find a solution */
  go();
}

void draw_queens(ModeInfo *mi) {
  Queenscreen *c = &qs[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if(!c->glx_context)
    return;

  glXMakeCurrent(disp, w, *(c->glx_context));

  display(c, MI_IS_WIREFRAME(mi));

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

void release_queens(ModeInfo *mi) {
  if(qs)
    free((void *) qs);

  FreeAllGL(MI);
}

#endif
