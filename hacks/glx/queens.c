/*
 * queens - solves n queens problem, displays
 * i make no claims that this is an optimal solution to the problem,
 * good enough for xss
 * hacked from glchess
 *
 * version 1.0 - May 10, 2002
 *
 * Copyright (C) 2002 Blair Tennessy (tennessy@cs.ubc.ca)
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
  {"+rotate", ".queens.rotate", XrmoptionNoArg, "false" },
  {"-rotate", ".queens.rotate", XrmoptionNoArg, "true" },
  {"+flat", ".queens.flat", XrmoptionNoArg, "false" },
  {"-flat", ".queens.flat", XrmoptionNoArg, "true" },
};

int rotate, wire, clearbits, flat;

static argtype vars[] = {
  {&rotate, "rotate", "Rotate", "True",  t_Bool},
  {&flat,   "flat",   "Flat",   "False", t_Bool},
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
#define COLORSETS 5

/* definition of white/black colors */
GLfloat colors[COLORSETS][2][3] = 
  { 
    {{0.43, 0.54, 0.76}, {0.8, 0.8, 0.8}},
    {{0.5, 0.7, 0.9}, {0.2, 0.3, 0.6}},
    {{0.53725490196, 0.360784313725, 0.521568627451}, {0.6, 0.6, 0.6}},
    {{0.15, 0.77, 0.54}, {0.5, 0.5, 0.5}},
    {{0.9, 0.45, 0.0}, {0.5, 0.5, 0.5}},
  };

int board[MAXBOARD][MAXBOARD];
int steps = 0, colorset = 0, BOARDSIZE = 8; /* 8 cuz its classic */
double theta = 0.0;

Bool
queens_handle_event (ModeInfo *mi, XEvent *event)
{
  Queenscreen *c = &qs[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      c->button_down_p = True;
      gltrackball_start (c->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      c->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (c->trackball, event->xbutton.button, 5,
                              !event->xbutton.state);
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
int findSolution(int row, int col) {
  if(row == BOARDSIZE)
    return 1;
  
  while(col < BOARDSIZE) {
    if(!conflicts(row, col)) {
      board[row][col] = 1;

      if(findSolution(row+1, 0))
        return 1;

      board[row][col] = 0;
    }

    ++col;
  }

  return 0;
}

/** driver for finding solution */
void go(void) { while(!findSolution(0, random()%BOARDSIZE)); }

/* lighting variables */
GLfloat front_shininess[] = {60.0};
GLfloat front_specular[] = {0.4, 0.4, 0.4, 1.0};
GLfloat ambient[] = {0.3, 0.3, 0.3, 1.0};
GLfloat diffuse[] = {0.8, 0.8, 0.8, 1.0};
GLfloat position[] = { 0.0, 5.0, 5.0, 1.0 };
GLfloat lmodel_ambient[] = {0.6, 0.6, 0.6, 1.0};
GLfloat lmodel_twoside[] = {GL_TRUE};

/* configure lighting */
void setup_lights(void) {

  /* setup twoside lighting */
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, position);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  /* setup material properties */
  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

#define    checkImageWidth 8
#define    checkImageHeight 8
GLubyte checkImage[checkImageWidth][checkImageHeight][3];

/* return alpha value for fading */
GLfloat findAlpha(void) {
  return steps < 128 ? steps/128.0 : steps < 1024-128 ?1.0:(1024-steps)/128.0;
}

/* draw pieces */
void drawPieces(void) {
  int i, j;

  for(i = 0; i < BOARDSIZE; ++i) {
    for(j = 0; j < BOARDSIZE; ++j) {
      if(board[i][j]) {
    	glColor3fv(colors[colorset][i%2]);
	glCallList(QUEEN);
      }
      
      glTranslatef(1.0, 0.0, 0.0);
    }
    
    glTranslatef(-1.0*BOARDSIZE, 0.0, 1.0);
  }
}

/** reflectionboard */
void draw_reflections(void) {
  int i, j;

  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, 1, 1);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
  glColorMask(0,0,0,0);
  glDisable(GL_CULL_FACE);

  glDisable(GL_DEPTH_TEST);
  glBegin(GL_QUADS);

  /* only draw white squares */
  for(i = 0; i < BOARDSIZE; ++i) {
    for(j = (BOARDSIZE+i) % 2; j < BOARDSIZE; j += 2) {
      glVertex3f(i, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j);
      glVertex3f(i, 0.0, j);
    }
  }
  glEnd();
  glEnable(GL_DEPTH_TEST);

  glColorMask(1, 1, 1, 1);
  glStencilFunc(GL_EQUAL, 1, 1);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  
  glPushMatrix(); 
  glScalef(1.0, -1.0, 1.0);
  glTranslatef(0.5, 0.001, 0.5);
  glLightfv(GL_LIGHT0, GL_POSITION, position);
  drawPieces();
  glPopMatrix();
  glDisable(GL_STENCIL_TEST);

  /* replace lights */
  glLightfv(GL_LIGHT0, GL_POSITION, position);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glColorMask(1,1,1,1);
}

/* draw board */
void drawBoard(void) {
  int i, j;

  glBegin(GL_QUADS);

  for(i = 0; i < BOARDSIZE; ++i)
    for(j = 0; j < BOARDSIZE; ++j) {
      int par = (i-j+BOARDSIZE)%2;
      glColor4f(colors[colorset][par][0],
		colors[colorset][par][1],
		colors[colorset][par][2],
		0.70);
      glNormal3f(0.0, 1.0, 0.0);
      glVertex3f(i, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j);
      glVertex3f(i, 0.0, j);
    }

  glEnd();
}

void display(Queenscreen *c) {
  glClear(clearbits);
  
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  /* setup light attenuation */
  glEnable(GL_COLOR_MATERIAL);
  glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.8/(0.01+findAlpha()));
  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.06);

  /** setup perspective */
  glTranslatef(0.0, 0.0, -1.5*BOARDSIZE);
  glRotatef(30.0, 1.0, 0.0, 0.0);
  gltrackball_rotate (c->trackball);
  glRotatef(theta*100, 0.0, 1.0, 0.0);
  glTranslatef(-0.5*BOARDSIZE, 0.0, -0.5*BOARDSIZE);

  /* find light positions */
  position[0] = BOARDSIZE/2.0 + BOARDSIZE/1.4*-sin(theta*100*M_PI/180.0);
  position[2] = BOARDSIZE/2.0 + BOARDSIZE/1.4*cos(theta*100*M_PI/180.0);
  position[1] = 6.0;

  if(!wire) {
    glEnable(GL_LIGHTING);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glEnable(GL_LIGHT0);
  }

  /* draw reflections */
  if(!wire) {
    draw_reflections();
    glEnable(GL_BLEND);
  }
  drawBoard();
  if(!wire)
    glDisable(GL_BLEND);

  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.1);

  glTranslatef(0.5, 0.0, 0.5);
  drawPieces();

  /* rotate camera */
  if(!c->button_down_p)
    theta += .002;

  /* zero out board, find new solution of size MINBOARD <= i <= MAXBOARD */
  if(++steps == 1024) {
    steps = 0;
    blank();
    BOARDSIZE = MINBOARD + (random() % (MAXBOARD - MINBOARD + 1));
    colorset = (colorset+1)%COLORSETS;
    go();
  }
}

int schunks = 15;
GLfloat spidermodel[][3] =
  {
    {0.48, 0.48, 0.22},
    {0.48, 0.34, 0.18},
    {0.34, 0.34, 0.10},
    {0.34, 0.18, 0.30},
    {0.18, 0.14, 0.38},
    {0.14, 0.29, 0.01},
    {0.29, 0.18, 0.18},
    {0.18, 0.18, 0.16},
    {0.18, 0.20, 0.26},
    {0.20, 0.27, 0.14},
    {0.27, 0.24, 0.08},
    {0.24, 0.17, 0.00},
    {0.17, 0.095, 0.08},
    {0.095, 0.07, 0.00},
    {0.07, 0.00, 0.12},
  };

#define EPSILON 0.001

/** draws cylindermodel */
void draw_model(int chunks, GLfloat model[][3], int r) {
  int i = 0;
  GLUquadricObj *quadric = gluNewQuadric();
  glPushMatrix();
  glRotatef(-90.0, 1.0, 0.0, 0.0);
  
  for(i = 0; i < chunks; ++i) {
    if(model[i][0] > EPSILON || model[i][1] > EPSILON)
      gluCylinder(quadric, model[i][0], model[i][1], model[i][2], r, 1);
    glTranslatef(0.0, 0.0, model[i][2]);
  }
  
  glPopMatrix();
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
  int screen = MI_SCREEN(mi);
  Queenscreen *c;
  wire = MI_IS_WIREFRAME(mi);

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
  glNewList(QUEEN, GL_COMPILE);
  draw_model(schunks, spidermodel, 24);
  glEndList();

  if(flat)
    glShadeModel(GL_FLAT);
  
  clearbits = GL_COLOR_BUFFER_BIT;

  glColorMaterial(GL_FRONT, GL_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);

  if(!wire) {
    setup_lights();
    glEnable(GL_DEPTH_TEST);
    clearbits |= GL_DEPTH_BUFFER_BIT;
    clearbits |= GL_STENCIL_BUFFER_BIT;
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
  }
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

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

  display(c);

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
