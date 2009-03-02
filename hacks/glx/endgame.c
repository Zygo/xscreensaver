/*
 * endgame.c
 * plays through a chess game ending.  enjoy.
 *
 * version 1.0 - June 6, 2002
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
# define PROGCLASS        "Endgame"
# define HACK_INIT        init_chess
# define HACK_DRAW        draw_chess
# define HACK_RESHAPE     reshape_chess
# define HACK_HANDLE_EVENT chess_handle_event
# define EVENT_MASK       PointerMotionMask
# define chess_opts  xlockmore_opts

#define DEFAULTS       "*delay:       20000       \n" \
                       "*showFPS:       False       \n" \
		       "*wireframe:	False     \n"	\
		       "*reflections:	True     \n"	\

# include "xlockmore.h"

#else
# include "xlock.h"
#endif

#ifdef USE_GL

#include <GL/glu.h>
#include "gltrackball.h"
#include "chessmodels.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static XrmOptionDescRec opts[] = {
  {"+rotate", ".chess.rotate", XrmoptionNoArg, (caddr_t) "false" },
  {"-rotate", ".chess.rotate", XrmoptionNoArg, (caddr_t) "true" },
  {"+reflections", ".chess.reflections", XrmoptionNoArg, (caddr_t) "false" },
  {"-reflections", ".chess.reflections", XrmoptionNoArg, (caddr_t) "true" },
  {"+smooth", ".chess.smooth", XrmoptionNoArg, (caddr_t) "false" },
  {"-smooth", ".chess.smooth", XrmoptionNoArg, (caddr_t) "true" },
};

int rotate, reflections, smooth;

static argtype vars[] = {
  {(caddr_t *) &rotate, "rotate", "Rotate", "True", t_Bool},
  {(caddr_t *) &reflections, "reflections", "Reflections", "True", t_Bool},
  {(caddr_t *) &smooth, "smooth", "Smooth", "True", t_Bool},
};

ModeSpecOpt chess_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   chess_description =
{"chess", "init_chess", "draw_chess", "release_chess",
 "draw_chess", "init_chess", NULL, &chess_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Chess", 0, NULL};

#endif

typedef struct {
  GLXContext *glx_context;
  Window window;
  trackball_state *trackball;
  Bool button_down_p;
} Chesscreen;

static Chesscreen *qs = NULL;

#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif

#define BOARDSIZE 8

/** definition of white/black (orange/gray) colors */
GLfloat colors[2][3] = 
  { 
    {1.0, 0.5, 0.0},
    {0.5, 0.5, 0.5},
  };

#define WHITES 5

/* i prefer silvertip */
GLfloat whites[WHITES][3] = 
  {
    {1.0, 0.5, 0.0},
    {0.8, 0.45, 1.0},
    {0.43, 0.54, 0.76},
    {0.8, 0.8, 0.8},
    {0.15, 0.77, 0.54},
  };

#include "chessgames.h"

ChessGame game;
int oldwhite = -1;

void build_colors(void) {

  /* find new white */
  int newwhite = oldwhite;
  while(newwhite == oldwhite)
    newwhite = random()%WHITES;
  oldwhite = newwhite;

  colors[0][0] = whites[oldwhite][0];
  colors[0][1] = whites[oldwhite][1];
  colors[0][2] = whites[oldwhite][2];
}

/* yay its c */
int mpiece = 0, tpiece, steps = 0, done = 1;
double from[2], to[2];
double dx, dz;
int moving = 0, take = 0, mc = 0, count = 99, wire = 0;

/** handle X event (trackball) */
Bool chess_handle_event (ModeInfo *mi, XEvent *event) {
  Chesscreen *c = &qs[MI_SCREEN(mi)];

  if(event->xany.type == ButtonPress && event->xbutton.button & Button1) {
    c->button_down_p = True;
    gltrackball_start (c->trackball,
		       event->xbutton.x, event->xbutton.y,
		       MI_WIDTH (mi), MI_HEIGHT (mi));
    return True;
  }
  else if(event->xany.type == ButtonRelease 
	  && event->xbutton.button & Button1) {
    c->button_down_p = False;
    return True;
  }
  else if(event->xany.type == MotionNotify && c->button_down_p) {
    gltrackball_track (c->trackball,
		       event->xmotion.x, event->xmotion.y,
		       MI_WIDTH (mi), MI_HEIGHT (mi));
    return True;
  }
  
  return False;
}

GLfloat position[] = { 0.0, 5.0, 5.0, 1.0 };
GLfloat position2[] = { 5.0, 5.0, 5.0, 1.0 };
GLfloat diffuse2[] = {1.0, 1.0, 1.0, 1.0};
GLfloat ambient2[] = {0.6, 0.6, 0.6, 1.0};

/* configure lighting */
void setup_lights(void) {
  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, position);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse2);
  glEnable(GL_LIGHT0);

/*   glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient2); */

  glLightfv(GL_LIGHT1, GL_SPECULAR, diffuse2);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse2);
  glEnable(GL_LIGHT1);
}

/** draw pieces */
void drawPieces(void) {
  int i, j;

  for(i = 0; i < BOARDSIZE; ++i) {
    for(j = 0; j < BOARDSIZE; ++j) {
      if(game.board[i][j]) {	
	int c = game.board[i][j]/PIECES;
	glColor3fv(colors[c]);
	glCallList(game.board[i][j]%PIECES);
      }
      
      glTranslatef(1.0, 0.0, 0.0);
    }
    
    glTranslatef(-1.0*BOARDSIZE, 0.0, 1.0);
  }

  glTranslatef(0.0, 0.0, -1.0*BOARDSIZE);
}

/** draw a moving piece */
void drawMovingPiece(void) {
  int piece = mpiece % PIECES;

  glPushMatrix();
  glColor3fv(colors[mpiece/PIECES]);

  /** assume a queening.  should be more general */
  if((mpiece == PAWN  && fabs(to[0]) < 0.01) || 
     (mpiece == BPAWN && fabs(to[0]-7.0) < 0.01)) {
    glTranslatef(from[1]+steps*dx, 0.0, from[0]+steps*dz);
    glColor4f(colors[mpiece/7][0], colors[mpiece/7][1], colors[mpiece/7][2],
	      (fabs(50.0-steps))/50.0);
    piece = steps < 50 ? PAWN : QUEEN;

    /* what a kludge */
    if(steps == 99)
      mpiece = mpiece == PAWN ? QUEEN : BQUEEN;
  }
  else if(mpiece % PIECES == KNIGHT) {
    glTranslatef(steps < 50 ? from[1] : to[1], 0.0, 
		 steps < 50 ? from[0] : to[0]);

    glColor4f(colors[mpiece/7][0], colors[mpiece/7][1], colors[mpiece/7][2],
	      fabs(49-steps)/49.0);
    glScalef(fabs(49-steps)/49.0, fabs(49-steps)/49.0, fabs(49-steps)/49.0);
  }
  else
    glTranslatef(from[1]+steps*dx, 0.0, from[0]+steps*dz);

  if(!wire)
    glEnable(GL_BLEND);
  
  glCallList(piece);
  glPopMatrix();

  if(!wire)
    glDisable(GL_BLEND);
}

/** code to squish a taken piece */
void drawTakePiece(void) {
  if(!wire)
    glEnable(GL_BLEND);

  glColor4f(colors[tpiece/7][0], colors[tpiece/7][1], colors[tpiece/7][2],
            (100-1.6*steps)/100.0);

  glTranslatef(to[1], 0.0, to[0]);
  
  if(mpiece % PIECES == KNIGHT)
    glScalef(1.0+steps/100.0, 1.0, 1.0+steps/100.0);
  else
    glScalef(1.0, 1 - steps/50.0 > 0.01 ? 1 - steps/50.0 : 0.01, 1.0);
  glCallList(tpiece % 7);
  
  if(!wire)
    glDisable(GL_BLEND);
}

/** draw board */
void drawBoard(void) {
  int i, j;

  glBegin(GL_QUADS);

  for(i = 0; i < BOARDSIZE; ++i)
    for(j = 0; j < BOARDSIZE; ++j) {
      /*glColor3fv(colors[(i+j)%2]);*/
      glColor4f(colors[(i+j)%2][0], colors[(i+j)%2][1],
		colors[(i+j)%2][2], 0.65);
      glNormal3f(0.0, 1.0, 0.0);
      glVertex3f(i, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j);
      glVertex3f(i, 0.0, j);
    }

  /* chop underneath board */
/*   glColor3f(0, 0, 0); */
/*   glNormal3f(0, -1, 0); */
/*   glVertex3f(0,         0,  BOARDSIZE); */
/*   glVertex3f(0,         0,  0); */
/*   glVertex3f(BOARDSIZE, 0,  0); */
/*   glVertex3f(BOARDSIZE, 0,  BOARDSIZE); */
  glEnd();
}

double theta = 0.0;

void draw_pieces(void) {
  drawPieces();
  if(moving) drawMovingPiece();
  if(take) drawTakePiece();
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
  glTranslatef(0.5, 0.0, 0.5);

  glLightfv(GL_LIGHT0, GL_POSITION, position);
  draw_pieces();
  glPopMatrix();
  
  glDisable(GL_STENCIL_TEST);
  glLightfv(GL_LIGHT0, GL_POSITION, position);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glColorMask(1,1,1,1);
}

/** draws the scene */
void display(Chesscreen *c) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  /** setup perspectif */
  glTranslatef(0.0, 0.0, -1.5*BOARDSIZE);
  glRotatef(30.0, 1.0, 0.0, 0.0);
  gltrackball_rotate (c->trackball);
  glRotatef(theta*100, 0.0, 1.0, 0.0);
  glTranslatef(-0.5*BOARDSIZE, 0.0, -0.5*BOARDSIZE);

  position[0] = 4.0 + 1.0*-sin(theta*100*M_PI/180.0);
  position[2] = 4.0 + 1.0*cos(theta*100*M_PI/180.0);
  position[1] = 8.0;

  position2[0] = 4.0 + 8.0*-sin(theta*100*M_PI/180.0);
  position2[2] = 4.0 + 8.0*cos(theta*100*M_PI/180.0);

  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, position);
  glLightfv(GL_LIGHT1, GL_POSITION, position2);
  glEnable(GL_LIGHT0);

  /** draw board, pieces */
  if(!wire) {
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);

    if(reflections) {
      draw_reflections();
      glEnable(GL_BLEND);
    }

    drawBoard();

    if(reflections)
      glDisable(GL_BLEND);
  }
  else
    drawBoard();
 
  glTranslatef(0.5, 0.0, 0.5);
  draw_pieces();

  if(!wire) {
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);
  }

  if (!c->button_down_p)
    theta += .002;
}

/** reshape handler */
void reshape_chess(ModeInfo *mi, int width, int height) {
  GLfloat h = (GLfloat) height / (GLfloat) width;
  glViewport(0,0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1/h, 2.0, 30.0);
  glMatrixMode(GL_MODELVIEW);
}

/** initialization handler */
void init_chess(ModeInfo *mi) {
  Chesscreen *c;
  int screen = MI_SCREEN(mi);
  wire = MI_IS_WIREFRAME(mi);

  if(!qs && 
     !(qs = (Chesscreen *) calloc(MI_NUM_SCREENS(mi), sizeof(Chesscreen))))
    return;
  
  c = &qs[screen];
  c->window = MI_WINDOW(mi);
  c->trackball = gltrackball_init ();
  
  if((c->glx_context = init_GL(mi)))
    reshape_chess(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  else
    MI_CLEARWINDOW(mi);

  glClearColor(0.0, 0.0, 0.0, 0.0);

  glDepthFunc(GL_LEQUAL);
  glClearStencil(0);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  gen_model_lists();

  if(!wire) {
    setup_lights();
    glColorMaterial(GL_FRONT, GL_DIFFUSE);
    glShadeModel(smooth ? GL_SMOOTH : GL_FLAT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
  }
  else
    glPolygonMode(GL_FRONT, GL_LINE);
}

int oldgame = -1;

/** does dirty work drawing scene, moving pieces */
void draw_chess(ModeInfo *mi) {
  Chesscreen *c = &qs[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if(!c->glx_context)
    return;

  glXMakeCurrent(disp, w, *(c->glx_context));

  /** code for moving a piece */
  if(moving && ++steps == 100) {
    moving = count = steps = take = 0;
    game.board[game.moves[mc][2]][game.moves[mc][3]] = mpiece;
    ++mc;
    
    if(mc == game.movecount) {
      done = 1;
      mc = 0;
    }
  }

  if(++count == 100) {
    if(!done) {
      mpiece = game.board[game.moves[mc][0]][game.moves[mc][1]];
      game.board[game.moves[mc][0]][game.moves[mc][1]] = NONE;
      
      if((tpiece = game.board[game.moves[mc][2]][game.moves[mc][3]])) {
	game.board[game.moves[mc][2]][game.moves[mc][3]] = NONE;
	take = 1;
      }
      
      from[0] = game.moves[mc][0];
      from[1] = game.moves[mc][1];
      to[0] = game.moves[mc][2];
      to[1] = game.moves[mc][3];
      
      dz = (to[0] - from[0]) / 100;
      dx = (to[1] - from[1]) / 100;
      steps = 0;
      moving = 1;
    }
    else if(done == 1) {
      int newgame = oldgame;
      while(newgame == oldgame)
	newgame = random()%GAMES;

      /* same old game */
      oldgame = newgame;
      game = games[oldgame];
      build_colors();
      done = 2;
      count = 0;
    }
    else {
      done = 0;
      count = 0;
    }
  }

  /* set lighting */
  if(done) {
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 
	     done == 1 ? 1.0+0.1*count : 100.0/count);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 
	     done == 1 ? 1.0+0.1*count : 100.0/count);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.15);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.15);
  }

  display(c);

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

/** bust it */
void release_chess(ModeInfo *mi) {
  if(qs)
    free((void *) qs);

  FreeAllGL(MI);
}

#endif
