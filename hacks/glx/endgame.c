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
};

int rotate, spidey, spideydark;

static argtype vars[] = {
  {(caddr_t *) &rotate, "rotate", "Rotate", "True", t_Bool},
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

/* ugggggggly */
#define NONE    0
#define KING    1
#define QUEEN   2
#define BISHOP  3 
#define KNIGHT  4 
#define ROOK    5
#define PAWN    6 
#define BKING    8
#define BQUEEN   9
#define BBISHOP  10 
#define BKNIGHT  11
#define BROOK    12
#define BPAWN    13 

#define BOARDSIZE 8
#define PIECES 7

/* definition of white/black colors */
GLfloat colors[2][3] = { {1.0, 0.5, 0.0},
			 {0.5, 0.5, 0.5} };

/* int board[8][8] = */
/*   { {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK}, */
/*     {PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN}, */
/*     {NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE}, */
/*     {NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE}, */
/*     {NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE}, */
/*     {NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE}, */
/*     {BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN}, */
/*     {BROOK, BKNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BKNIGHT, BROOK}}; */

int board[8][8];

void buildBoard(void) {
  board[0][5] = BKING;
  board[1][4] = BPAWN;
  board[1][2] = BPAWN;
  board[1][0] = BPAWN;
  board[2][2] = BPAWN;
  board[2][4] = BPAWN;
  board[2][7] = KNIGHT;
  board[3][0] = PAWN;
  board[3][2] = ROOK;
  board[4][0] = PAWN;
  board[4][4] = KING;
  board[4][5] = PAWN;
  board[6][0] = BPAWN;
  board[6][7] = PAWN;
  board[7][0] = BBISHOP;
}

#define MOVES 24

int moves[MOVES][4] = 
  { {3, 2, 6, 2},
    {7, 0, 6, 1},
    {6, 2, 6, 6},
    {0, 5, 0, 4},
    {6, 6, 0, 6},
    {0, 4, 1, 3},
    {2, 7, 1, 5},
    {2, 2, 3, 2},
    {0, 6, 0, 3}, 
    {1, 3, 2, 2},
    {0, 3, 6, 3},
    {3, 2, 4, 2}, /* pawn to bishop 5 */
    {1, 5, 0, 3}, /* check */
    {2, 2, 3, 2},
    {0, 3, 2, 4}, /* takes pawn */
    {3, 2, 2, 2},
    {2, 4, 0, 3},
    {2, 2, 3, 2},
    {6, 3, 6, 1}, /* rook takes bishop */
    {6, 0, 7, 0}, /* hack this in! */
    {6, 1, 3, 1},
    {3, 2, 2, 3},
    {3, 1, 3, 3},
    {0, 0, 2, 3},
  };

/* yay its c */
int mpiece = 0, tpiece, steps = 0;
double mcount = 0.0;
double from[2], to[2];
double dx, dz;
int moving = 0, take = 0, mc = 0, count = 0;

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

/* clear board */
void blank(void) {
  int i, j;

  for(i = 0; i < BOARDSIZE; ++i)
    for(j = 0; j < BOARDSIZE; ++j)
      board[i][j] = NONE;
}

/* configure lighting */
void setup_lights(void) {
  GLfloat position[] = { 0.0, 8.0, 0.0, 1.0 };

  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, position);
  glEnable(GL_LIGHT0);
}

/* draw pieces */
void drawPieces(void) {
  int i, j;

  for(i = 0; i < BOARDSIZE; ++i) {
    for(j = 0; j < BOARDSIZE; ++j) {
      if(board[i][j]) {	
	int c = board[i][j]/PIECES;
	glColor3fv(colors[c]);
	glCallList(board[i][j]%7);
      }
      
      glTranslatef(1.0, 0.0, 0.0);
    }
    
    glTranslatef(-1.0*BOARDSIZE, 0.0, 1.0);
  }

  glTranslatef(0.0, 0.0, -1.0*BOARDSIZE);
}

void drawMovingPiece(int wire) {
  glTranslatef(from[1], 0.0, from[0]);
  glColor3fv(colors[mpiece/7]);

  /* assume a queening.  should be more general */
  if((mpiece == PAWN  && fabs(to[0]) < 0.01) || 
     (mpiece == BPAWN && fabs(to[0]-7.0) < 0.01)) {
    if(!wire)
      glEnable(GL_BLEND);

    glColor4f(colors[mpiece/7][0], colors[mpiece/7][1], colors[mpiece/7][2],
	      (fabs(50.0-steps))/50.0);
    
    glCallList(steps < 50 ? PAWN : QUEEN);

    /* what a kludge.  yay for side effects */
    if(steps == 99)
      mpiece = mpiece == PAWN ? QUEEN : BQUEEN;

    if(!wire)
      glDisable(GL_BLEND);
  }
  else
    glCallList(mpiece % 7);
}

void drawTakePiece(int wire) {
  if(!wire)
    glEnable(GL_BLEND);

  glColor4f(colors[tpiece/7][0], colors[tpiece/7][1], colors[tpiece/7][2],
            (100-1.6*steps)/100.0);

  glTranslatef(to[1] - from[1], 0.0, to[0] - from[0]);
  glScalef(1.0, 1 - steps/50.0 > 0.01 ? 1 - steps/50.0 : 0.01, 1.0);
  glCallList(tpiece % 7);
  
  if(!wire)
    glDisable(GL_BLEND);
}

/* draw board */
void drawBoard(void) {
  int i, j;

  for(i = 0; i < BOARDSIZE; ++i)
    for(j = 0; j < BOARDSIZE; ++j) {
      glColor3fv(colors[(i+j)%2]);
      glNormal3f(0.0, 1.0, 0.0);

      glBegin(GL_QUADS);

      glVertex3f(i - 0.5, 0.0, j + 0.5);
      glVertex3f(i + 0.5, 0.0, j + 0.5);
      glVertex3f(i + 0.5, 0.0, j - 0.5);
      glVertex3f(i - 0.5, 0.0, j - 0.5);

      /* draw the bottom, too */
      glNormal3f(0.0, -1.0, 0.0);

      glVertex3f(i - 0.5, 0.0, j - 0.5);
      glVertex3f(i + 0.5, 0.0, j - 0.5);
      glVertex3f(i + 0.5, 0.0, j + 0.5);
      glVertex3f(i - 0.5, 0.0, j + 0.5);

      glEnd();
    }
}

#define SQ 0.5

double theta = 0.0;

void display(Chesscreen *c, int wire) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glTranslatef(0.0, -3.0+fabs(sin(theta)), -1.3*BOARDSIZE);
  gltrackball_rotate (c->trackball);
  glRotatef(theta*100, 0.0, 1.0, 0.0);
  glTranslatef(-0.5*(BOARDSIZE-1), 0.0, -0.5*(BOARDSIZE-1));

  if(!wire) {
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
  }

  drawBoard();
  glTranslatef(0.0, .02, 0.0);
  drawPieces();
  if(moving) drawMovingPiece(wire);
  if(take) drawTakePiece(wire);

  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_LIGHTING);
  
  theta += .002;
}

void reshape_chess(ModeInfo *mi, int width, int height) {
  GLfloat h = (GLfloat) height / (GLfloat) width;
  glViewport(0,0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1/h, 2.0, 30.0);
  glMatrixMode(GL_MODELVIEW);
}

void init_chess(ModeInfo *mi) {
  GLfloat mat_shininess[] = { 90.0 };
  GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };

  int screen = MI_SCREEN(mi);
  int wire = MI_IS_WIREFRAME(mi);
  Chesscreen *c;

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

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glLineWidth(1.0);
  glDepthFunc(GL_LEQUAL);

  setup_lights();

  gen_model_lists();

  if (!wire) {
    glColorMaterial(GL_FRONT, GL_DIFFUSE);

    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
    glShadeModel(GL_SMOOTH);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
  }
  else
    glPolygonMode(GL_FRONT, GL_LINE);

  buildBoard();
}

void draw_chess(ModeInfo *mi) {
  Chesscreen *c = &qs[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if(!c->glx_context)
    return;

  glXMakeCurrent(disp, w, *(c->glx_context));

  /* moving code */
  if(moving) {
    ++steps;
    from[0] += dz;
    from[1] += dx;
  }
  
  if(steps == 100) {
    moving = count = steps = take = 0;
    board[moves[mc][2]][moves[mc][3]] = mpiece;
    ++mc;
    
    if(mc == MOVES) {
      blank();
      buildBoard();
      mc = 0;
    }
  }

  if(count++ == 100) {
    moving = 1;
    mpiece = board[moves[mc][0]][moves[mc][1]];
    board[moves[mc][0]][moves[mc][1]] = NONE;

    if((tpiece = board[moves[mc][2]][moves[mc][3]]) != NONE) {
      board[moves[mc][2]][moves[mc][3]] = NONE;
      take = 1;
    }
    
    mcount = 0.0;
    from[0] = moves[mc][0];
    from[1] = moves[mc][1];
    to[0] = moves[mc][2];
    to[1] = moves[mc][3];
    
    dz = (to[0] - from[0]) / 100;
    dx = (to[1] - from[1]) / 100;
    steps = 0;
  }

  display(c, MI_IS_WIREFRAME(mi));

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

void release_chess(ModeInfo *mi) {
  if(qs)
    free((void *) qs);

  FreeAllGL(MI);
}

#endif
