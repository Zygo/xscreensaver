/*
 * endgame.c
 * plays through a chess game ending.  enjoy.
 *
 * version 1.0 - June 6, 2002
 *
 * Copyright (C) 2002-2007 Blair Tennessy (tennessb@unbc.ca)
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifdef STANDALONE
#define DEFAULTS       "*delay:     20000 \n" \
                       "*showFPS:   False \n" \
		       "*wireframe: False \n" \

# define refresh_chess 0
# include "xlockmore.h"

#else
# include "xlock.h"
#endif

#ifdef USE_GL

#define BOARDSIZE 8
#define WHITES 5

#include "gltrackball.h"
#include "chessmodels.h"
#include "chessgames.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static XrmOptionDescRec opts[] = {
  {"+rotate", ".chess.rotate", XrmoptionNoArg, "false" },
  {"-rotate", ".chess.rotate", XrmoptionNoArg, "true" },
  {"+reflections", ".chess.reflections", XrmoptionNoArg, "false" },
  {"-reflections", ".chess.reflections", XrmoptionNoArg, "true" },
  {"+shadows", ".chess.shadows", XrmoptionNoArg, "false" },
  {"-shadows", ".chess.shadows", XrmoptionNoArg, "true" },
  {"+smooth", ".chess.smooth", XrmoptionNoArg, "false" },
  {"-smooth", ".chess.smooth", XrmoptionNoArg, "true" },
  {"+classic", ".chess.classic", XrmoptionNoArg, "false" },
  {"-classic", ".chess.classic", XrmoptionNoArg, "true" },
};

static int rotate, reflections, smooth, shadows, classic;

static argtype vars[] = {
  {&rotate,      "rotate",      "Rotate",      "True", t_Bool},
  {&reflections, "reflections", "Reflections", "True", t_Bool},
  {&shadows, "shadows", "Shadows", "True", t_Bool},
  {&smooth,      "smooth",      "Smooth",      "True", t_Bool},
  {&classic,     "classic",     "Classic",     "False", t_Bool},
};

ENTRYPOINT ModeSpecOpt chess_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   chess_description =
{"chess", "init_chess", "draw_chess", "release_chess",
 "draw_chess", "init_chess", NULL, &chess_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Chess", 0, NULL};

#endif

#define checkImageWidth 16
#define checkImageHeight 16

typedef struct {
  GLXContext *glx_context;
  Window window;
  trackball_state *trackball;
  Bool button_down_p;

  ChessGame game;
  int oldwhite;

  /** definition of white/black (orange/gray) colors */
  GLfloat colors[2][3];

  GLubyte checkImage[checkImageWidth][checkImageHeight][3];
  GLuint piecetexture, boardtexture;

  int mpiece, tpiece, steps, done;
  double from[2], to[2];
  double dx, dz;
  int moving, take, mc, count, wire;
  double theta;

  GLfloat position[4];
  GLfloat position2[4];

  GLfloat mod;

  GLfloat ground[4];

  int oldgame;

  int poly_counts[PIECES];  /* polygon count of each type of piece */


} Chesscreen;

static Chesscreen *qs = NULL;

static const GLfloat MaterialShadow[] =   {0.0, 0.0, 0.0, 0.3};


/* i prefer silvertip */
static const GLfloat whites[WHITES][3] = 
  {
    {1.0, 0.55, 0.1},
    {0.8, 0.52, 0.8},
    {0.43, 0.54, 0.76},
    {0.8, 0.8, 0.8},
    {0.35, 0.60, 0.35},
  };

static void build_colors(Chesscreen *cs) 
{

  /* find new white */
  int newwhite = cs->oldwhite;
  while(newwhite == cs->oldwhite)
    newwhite = random()%WHITES;
  cs->oldwhite = newwhite;

  cs->colors[0][0] = whites[cs->oldwhite][0];
  cs->colors[0][1] = whites[cs->oldwhite][1];
  cs->colors[0][2] = whites[cs->oldwhite][2];
}

/* build piece texture */
static void make_piece_texture(Chesscreen *cs) 
{
  int i, j, c;

  for (i = 0; i < checkImageWidth; i++) {
    for (j = 0; j < checkImageHeight; j++) {
      c = ((j%2) == 0 || i%2 == 0) ? 240 : 180+random()%16;
      cs->checkImage[i][j][0] = (GLubyte) c;
      cs->checkImage[i][j][1] = (GLubyte) c;
      cs->checkImage[i][j][2] = (GLubyte) c;
    }
  }

  glGenTextures(1, &cs->piecetexture);
  glBindTexture(GL_TEXTURE_2D, cs->piecetexture);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, 3, checkImageWidth, 
	       checkImageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 
	       &cs->checkImage[0][0]);
}

/* build board texture (uniform noise in [180,180+50]) */
static void make_board_texture(Chesscreen *cs) 
{
  int i, j, c;

  for (i = 0; i < checkImageWidth; i++) {
    for (j = 0; j < checkImageHeight; j++) {
      c = 180 + random()%51;
      cs->checkImage[i][j][0] = (GLubyte) c;
      cs->checkImage[i][j][1] = (GLubyte) c;
      cs->checkImage[i][j][2] = (GLubyte) c;
    }
  }

  glGenTextures(1, &cs->boardtexture);
  glBindTexture(GL_TEXTURE_2D, cs->boardtexture);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, 3, checkImageWidth, 
	       checkImageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 
	       &cs->checkImage[0][0]);
}

/** handle X event (trackball) */
ENTRYPOINT Bool chess_handle_event (ModeInfo *mi, XEvent *event) 
{
  Chesscreen *cs = &qs[MI_SCREEN(mi)];

  if(event->xany.type == ButtonPress && event->xbutton.button == Button1) {
    cs->button_down_p = True;
    gltrackball_start (cs->trackball,
		       event->xbutton.x, event->xbutton.y,
		       MI_WIDTH (mi), MI_HEIGHT (mi));
    return True;
  }
  else if(event->xany.type == ButtonRelease 
	  && event->xbutton.button == Button1) {
    cs->button_down_p = False;
    return True;
  }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (cs->trackball, event->xbutton.button, 5,
                              !event->xbutton.state);
      return True;
    }
  else if(event->xany.type == MotionNotify && cs->button_down_p) {
    gltrackball_track (cs->trackball,
		       event->xmotion.x, event->xmotion.y,
		       MI_WIDTH (mi), MI_HEIGHT (mi));
    return True;
  }
  
  return False;
}

static const GLfloat diffuse2[] = {1.0, 1.0, 1.0, 1.0};
/*static const GLfloat ambient2[] = {0.7, 0.7, 0.7, 1.0};*/
static const GLfloat shininess[] = {60.0};
static const GLfloat specular[] = {0.4, 0.4, 0.4, 1.0};

/* configure lighting */
static void setup_lights(Chesscreen *cs) 
{
  glEnable(GL_LIGHTING);
  glLightfv(GL_LIGHT0, GL_POSITION, cs->position);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse2);
  glEnable(GL_LIGHT0);

/*   glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient2); */

  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);

  glLightfv(GL_LIGHT1, GL_SPECULAR, diffuse2);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse2);
  glEnable(GL_LIGHT1);
}

/* draw pieces */
static void drawPieces(ModeInfo *mi, Chesscreen *cs) 
{
  int i, j;

  for(i = 0; i < BOARDSIZE; ++i) {
    for(j = 0; j < BOARDSIZE; ++j) {
      if(cs->game.board[i][j]) {	
	int c = cs->game.board[i][j]/PIECES;
	glColor3fv(cs->colors[c]);
	glCallList(cs->game.board[i][j]%PIECES);
        mi->polygon_count += cs->poly_counts[cs->game.board[i][j]%PIECES];
      }
      
      glTranslatef(1.0, 0.0, 0.0);
    }
    
    glTranslatef(-1.0*BOARDSIZE, 0.0, 1.0);
  }

  glTranslatef(0.0, 0.0, -1.0*BOARDSIZE);
}

/* draw pieces */
static void drawPiecesShadow(ModeInfo *mi, Chesscreen *cs) 
{
  int i, j;

  for(i = 0; i < BOARDSIZE; ++i) {
    for(j = 0; j < BOARDSIZE; ++j) {
      if(cs->game.board[i][j]) {	
	glColor4f(0.0, 0.0, 0.0, 0.4);
	glCallList(cs->game.board[i][j]%PIECES);
        mi->polygon_count += cs->poly_counts[cs->game.board[i][j]%PIECES];
      }
      
      glTranslatef(1.0, 0.0, 0.0);
    }
    
    glTranslatef(-1.0*BOARDSIZE, 0.0, 1.0);
  }

  glTranslatef(0.0, 0.0, -1.0*BOARDSIZE);
}

/* draw a moving piece */
static void drawMovingPiece(ModeInfo *mi, Chesscreen *cs, int shadow) 
{
  int piece = cs->mpiece % PIECES;

  glPushMatrix();

  if(shadow) glColor4fv(MaterialShadow);
  else glColor3fv(cs->colors[cs->mpiece/PIECES]);

  /** assume a queening.  should be more general */
  if((cs->mpiece == PAWN  && fabs(cs->to[0]) < 0.01) || 
     (cs->mpiece == BPAWN && fabs(cs->to[0]-7.0) < 0.01)) {
    glTranslatef(cs->from[1]+cs->steps*cs->dx, 0.0, cs->from[0]+cs->steps*cs->dz);

    glColor4f(shadow ? MaterialShadow[0] : cs->colors[cs->mpiece/7][0], 
	      shadow ? MaterialShadow[1] : cs->colors[cs->mpiece/7][1], 
	      shadow ? MaterialShadow[2] : cs->colors[cs->mpiece/7][2],
	      (fabs(50.0-cs->steps))/50.0);

    piece = cs->steps < 50 ? PAWN : QUEEN;

    /* what a kludge */
    if(cs->steps == 99)
      cs->mpiece = cs->mpiece == PAWN ? QUEEN : BQUEEN;
  }
  else if(cs->mpiece % PIECES == KNIGHT) {
    GLfloat shine[1];
    GLfloat spec[4];
    GLfloat mult;
    glTranslatef(cs->steps < 50 ? cs->from[1] : cs->to[1], 0.0, 
		 cs->steps < 50 ? cs->from[0] : cs->to[0]);

    mult = cs->steps < 10 
      ? (1.0 - cs->steps / 10.0) : 100 - cs->steps < 10 
      ? (1.0 - (100 - cs->steps) / 10.0) : 0.0;

    shine[0] = mult*shininess[0];
    spec[0] = mult*specular[0];
    spec[1] = mult*specular[1];
    spec[2] = mult*specular[2];
    spec[3] = 1.0;
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shine);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);

    glColor4f(shadow ? MaterialShadow[0] : cs->colors[cs->mpiece/7][0], 
	      shadow ? MaterialShadow[1] : cs->colors[cs->mpiece/7][1], 
	      shadow ? MaterialShadow[2] : cs->colors[cs->mpiece/7][2],
	      fabs(49-cs->steps)/49.0);

    glScalef(fabs(49-cs->steps)/49.0, fabs(49-cs->steps)/49.0, fabs(49-cs->steps)/49.0);
  }
  else
    glTranslatef(cs->from[1]+cs->steps*cs->dx, 0.0, cs->from[0]+cs->steps*cs->dz);

  if(!cs->wire)
    glEnable(GL_BLEND);
  
  glCallList(piece);
  mi->polygon_count += cs->poly_counts[cs->mpiece % PIECES];

  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);

  glPopMatrix();

  if(!cs->wire)
    glDisable(GL_BLEND);
}

/** code to squish a taken piece */
static void drawTakePiece(ModeInfo *mi, Chesscreen *cs, int shadow) 
{
  if(!cs->wire)
    glEnable(GL_BLEND);

  glColor4f(shadow ? MaterialShadow[0] : cs->colors[cs->tpiece/7][0], 
	    shadow ? MaterialShadow[1] : cs->colors[cs->tpiece/7][1], 
	    shadow ? MaterialShadow[2] : cs->colors[cs->tpiece/7][2],
            (100-1.6*cs->steps)/100.0);

  glTranslatef(cs->to[1], 0.0, cs->to[0]);
  
  if(cs->mpiece % PIECES == KNIGHT)
    glScalef(1.0+cs->steps/100.0, 1.0, 1.0+cs->steps/100.0);
  else
    glScalef(1.0, 1 - cs->steps/50.0 > 0.01 ? 1 - cs->steps/50.0 : 0.01, 1.0);
  glCallList(cs->tpiece % 7);
  mi->polygon_count += cs->poly_counts[cs->tpiece % PIECES];
  
  if(!cs->wire)
    glDisable(GL_BLEND);
}

/** draw board */
static void drawBoard(ModeInfo *mi, Chesscreen *cs) 
{
  int i, j;

  glBegin(GL_QUADS);

  for(i = 0; i < BOARDSIZE; ++i)
    for(j = 0; j < BOARDSIZE; ++j) {
      double ma1 = (i+j)%2 == 0 ? cs->mod*i : 0.0;
      double mb1 = (i+j)%2 == 0 ? cs->mod*j : 0.0;
      double ma2 = (i+j)%2 == 0 ? cs->mod*(i+1.0) : 0.01;
      double mb2 = (i+j)%2 == 0 ? cs->mod*(j+1.0) : 0.01;

      /*glColor3fv(colors[(i+j)%2]);*/
      glColor4f(cs->colors[(i+j)%2][0], cs->colors[(i+j)%2][1],
		cs->colors[(i+j)%2][2], 0.65);
      
      glNormal3f(0.0, 1.0, 0.0);
/*       glTexCoord2f(mod*i, mod*(j+1.0)); */
      glTexCoord2f(ma1, mb2);
      glVertex3f(i, 0.0, j + 1.0);
/*       glTexCoord2f(mod*(i+1.0), mod*(j+1.0)); */
      glTexCoord2f(ma2, mb2);
      glVertex3f(i + 1.0, 0.0, j + 1.0);
      glTexCoord2f(ma2, mb1);
/*       glTexCoord2f(mod*(i+1.0), mod*j); */
      glVertex3f(i + 1.0, 0.0, j);
      glTexCoord2f(ma1, mb1);
/*       glTexCoord2f(mod*i, mod*j); */
      glVertex3f(i, 0.0, j);

      mi->polygon_count++;
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

static void draw_pieces(ModeInfo *mi, Chesscreen *cs, int wire) 
{
  if (!cs->wire) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cs->piecetexture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glColor4f(0.5, 0.5, 0.5, 1.0);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialShadow);
  }

  drawPieces(mi, cs);
  if(cs->moving) drawMovingPiece(mi, cs, 0);
  if(cs->take) drawTakePiece(mi, cs, 0);
  glDisable(GL_TEXTURE_2D);
}

static void draw_shadow_pieces(ModeInfo *mi, Chesscreen *cs, int wire) 
{
  if (!cs->wire) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cs->piecetexture);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  }

  /* use the stencil */
  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);

  glClear(GL_STENCIL_BUFFER_BIT);
  glColorMask(0,0,0,0);
  glEnable(GL_STENCIL_TEST);

  glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFFL);
  glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
  

  glPushMatrix();
  glTranslatef(0.0, 0.001, 0.0);

  /* draw the pieces */
  drawPiecesShadow(mi, cs);
  if(cs->moving) drawMovingPiece(mi, cs, shadows);
  if(cs->take) drawTakePiece(mi, cs, shadows);

  glPopMatrix();


  /* turn on drawing into colour buffer */
  glColorMask(1,1,1,1);

  /* programming with effect */
  glDisable(GL_LIGHTING);
  glDisable(GL_COLOR_MATERIAL);
  glDisable(GL_TEXTURE_2D);

  /* now draw the union of the shadows */

  /* 
     <todo>
     want to keep alpha values (alpha is involved in transition
     effects of the active pieces).
     </todo>
  */
  glStencilFunc(GL_NOTEQUAL, 0, 0xFFFFFFFFL);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

  glEnable(GL_BLEND);

  glColor4fv(MaterialShadow);

  /* draw the board generously to fill the shadows */
  glBegin(GL_QUADS);

  glVertex3f(-1.0, 0.0, -1.0);
  glVertex3f(-1.0, 0.0, BOARDSIZE + 1.0);
  glVertex3f(1.0 + BOARDSIZE, 0.0, BOARDSIZE + 1.0);
  glVertex3f(1.0 + BOARDSIZE, 0.0, -1.0);

  glEnd();

  glDisable(GL_STENCIL_TEST);

  /* "pop" attributes */
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_COLOR_MATERIAL);
  glEnable(GL_LIGHTING);
  glEnable(GL_CULL_FACE);



}

enum {X, Y, Z, W};
enum {A, B, C, D};

/* create a matrix that will project the desired shadow */
static void shadowmatrix(GLfloat shadowMat[4][4],
		  GLfloat groundplane[4],
		  GLfloat lightpos[4]) 
{
  GLfloat dot;

  /* find dot product between light position vector and ground plane normal */
  dot = groundplane[X] * lightpos[X] +
        groundplane[Y] * lightpos[Y] +
        groundplane[Z] * lightpos[Z] +
        groundplane[W] * lightpos[W];

  shadowMat[0][0] = dot - lightpos[X] * groundplane[X];
  shadowMat[1][0] = 0.f - lightpos[X] * groundplane[Y];
  shadowMat[2][0] = 0.f - lightpos[X] * groundplane[Z];
  shadowMat[3][0] = 0.f - lightpos[X] * groundplane[W];

  shadowMat[X][1] = 0.f - lightpos[Y] * groundplane[X];
  shadowMat[1][1] = dot - lightpos[Y] * groundplane[Y];
  shadowMat[2][1] = 0.f - lightpos[Y] * groundplane[Z];
  shadowMat[3][1] = 0.f - lightpos[Y] * groundplane[W];

  shadowMat[X][2] = 0.f - lightpos[Z] * groundplane[X];
  shadowMat[1][2] = 0.f - lightpos[Z] * groundplane[Y];
  shadowMat[2][2] = dot - lightpos[Z] * groundplane[Z];
  shadowMat[3][2] = 0.f - lightpos[Z] * groundplane[W];

  shadowMat[X][3] = 0.f - lightpos[W] * groundplane[X];
  shadowMat[1][3] = 0.f - lightpos[W] * groundplane[Y];
  shadowMat[2][3] = 0.f - lightpos[W] * groundplane[Z];
  shadowMat[3][3] = dot - lightpos[W] * groundplane[W];
}

/** reflectionboard */
static void draw_reflections(ModeInfo *mi, Chesscreen *cs) 
{
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
      mi->polygon_count++;
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

  glLightfv(GL_LIGHT0, GL_POSITION, cs->position);
  draw_pieces(mi, cs, cs->wire);
  glPopMatrix();
  
  glDisable(GL_STENCIL_TEST);
  glLightfv(GL_LIGHT0, GL_POSITION, cs->position);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glColorMask(1,1,1,1);
}

/** draws the scene */
static void display(ModeInfo *mi, Chesscreen *cs) 
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  mi->polygon_count = 0;

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  /** setup perspectiv */
  glTranslatef(0.0, 0.0, -1.5*BOARDSIZE);
  glRotatef(30.0, 1.0, 0.0, 0.0);
  gltrackball_rotate (cs->trackball);
  glRotatef(cs->theta*100, 0.0, 1.0, 0.0);
  glTranslatef(-0.5*BOARDSIZE, 0.0, -0.5*BOARDSIZE);

/*   cs->position[0] = 4.0 + 1.0*-sin(cs->theta*100*M_PI/180.0); */
/*   cs->position[2] = 4.0 + 1.0* cos(cs->theta*100*M_PI/180.0); */
/*   cs->position[1] = 5.0; */

  /* this is the lone light that the shadow matrix is generated from */
  cs->position[0] = 1.0;
  cs->position[2] = 1.0;
  cs->position[1] = 16.0;

  cs->position2[0] = 4.0 + 8.0*-sin(cs->theta*100*M_PI/180.0);
  cs->position2[2] = 4.0 + 8.0* cos(cs->theta*100*M_PI/180.0);

  if (!cs->wire) {
    glEnable(GL_LIGHTING);
    glLightfv(GL_LIGHT0, GL_POSITION, cs->position);
    glLightfv(GL_LIGHT1, GL_POSITION, cs->position2);
    glEnable(GL_LIGHT0);
  }

  /** draw board, pieces */
  if(!cs->wire) {
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);

    if(reflections && !cs->wire) {
      draw_reflections(mi, cs);
      glEnable(GL_BLEND);
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cs->boardtexture);
    drawBoard(mi, cs);
    glDisable(GL_TEXTURE_2D);

    if(shadows) {
      /* render shadows */
      GLfloat m[4][4];
      shadowmatrix(m, cs->ground, cs->position);

      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialShadow);
      glEnable(GL_BLEND);
      glDisable(GL_LIGHTING);
      glDisable(GL_DEPTH_TEST);
      
      /* display ant shadow */
      glPushMatrix();
      glTranslatef(0.0, 0.001, 0.0);
      glMultMatrixf(m[0]);
      glTranslatef(0.5, 0.01, 0.5);
      draw_shadow_pieces(mi, cs, cs->wire);
      glPopMatrix();      

      glEnable(GL_LIGHTING);
      glDisable(GL_BLEND);
      glEnable(GL_DEPTH_TEST);
    }

    if(reflections)
      glDisable(GL_BLEND);
  }
  else
    drawBoard(mi, cs);
 
  glTranslatef(0.5, 0.0, 0.5);
  draw_pieces(mi, cs, cs->wire);

  if(!cs->wire) {
    glDisable(GL_COLOR_MATERIAL);
    glDisable(GL_LIGHTING);
  }

  if (!cs->button_down_p)
    cs->theta += .002;
}

/** reshape handler */
ENTRYPOINT void reshape_chess(ModeInfo *mi, int width, int height) 
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  glViewport(0,0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1/h, 2.0, 30.0);
  glMatrixMode(GL_MODELVIEW);
}

/** initialization handler */
ENTRYPOINT void init_chess(ModeInfo *mi) 
{
  Chesscreen *cs;
  int screen = MI_SCREEN(mi);

  if(!qs && 
     !(qs = (Chesscreen *) calloc(MI_NUM_SCREENS(mi), sizeof(Chesscreen))))
    return;
  
  cs = &qs[screen];
  cs->window = MI_WINDOW(mi);
  cs->wire = MI_IS_WIREFRAME(mi);
  cs->trackball = gltrackball_init ();
  
  cs->oldwhite = -1;

  cs->colors[0][0] = 1.0;
  cs->colors[0][1] = 0.5;
  cs->colors[0][2] = 0.0;

  cs->colors[1][0] = 0.6;
  cs->colors[1][1] = 0.6;
  cs->colors[1][2] = 0.6;

  cs->done = 1;
  cs->count = 99;
  cs->mod = 1.4;

/*   cs->position[0] = 0.0; */
/*   cs->position[1] = 5.0; */
/*   cs->position[2] = 5.0; */
/*   cs->position[3] = 1.0; */

  cs->position[0] = 0.0;
  cs->position[1] = 24.0;
  cs->position[2] = 2.0;
  cs->position[3] = 1.0;


  cs->position2[0] = 5.0;
  cs->position2[1] = 5.0;
  cs->position2[2] = 5.0;
  cs->position2[3] = 1.0;

  cs->ground[0] = 0.0;
  cs->ground[1] = 1.0;
  cs->ground[2] = 0.0;
  cs->ground[3] = -0.00001;

  cs->oldgame = -1;


  if((cs->glx_context = init_GL(mi)))
    reshape_chess(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  else
    MI_CLEARWINDOW(mi);

  glClearColor(0.0, 0.0, 0.0, 0.0);

  if (!cs->wire) {
    glDepthFunc(GL_LEQUAL);
    glClearStencil(0);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    make_piece_texture(cs);
    make_board_texture(cs);
  }
  gen_model_lists( classic, cs->poly_counts);

  if(!cs->wire) {
    setup_lights(cs);
    glColorMaterial(GL_FRONT, GL_DIFFUSE);
    glShadeModel(smooth ? GL_SMOOTH : GL_FLAT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
  }
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

/** does dirty work drawing scene, moving pieces */
ENTRYPOINT void draw_chess(ModeInfo *mi) 
{
  Chesscreen *cs = &qs[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if(!cs->glx_context)
    return;

  glXMakeCurrent(disp, w, *(cs->glx_context));

  /** code for moving a piece */
  if(cs->moving && ++cs->steps == 100) {
    cs->moving = cs->count = cs->steps = cs->take = 0;
    cs->game.board[cs->game.moves[cs->mc][2]][cs->game.moves[cs->mc][3]] = cs->mpiece;
    ++cs->mc;
    
    if(cs->mc == cs->game.movecount) {
      cs->done = 1;
      cs->mc = 0;
    }
  }

  if(++cs->count == 100) {
    if(!cs->done) {
      cs->mpiece = cs->game.board[cs->game.moves[cs->mc][0]][cs->game.moves[cs->mc][1]];
      cs->game.board[cs->game.moves[cs->mc][0]][cs->game.moves[cs->mc][1]] = NONE;
      
      if((cs->tpiece = cs->game.board[cs->game.moves[cs->mc][2]][cs->game.moves[cs->mc][3]])) {
	cs->game.board[cs->game.moves[cs->mc][2]][cs->game.moves[cs->mc][3]] = NONE;
	cs->take = 1;
      }
      
      cs->from[0] = cs->game.moves[cs->mc][0];
      cs->from[1] = cs->game.moves[cs->mc][1];
      cs->to[0] = cs->game.moves[cs->mc][2];
      cs->to[1] = cs->game.moves[cs->mc][3];
      
      cs->dz = (cs->to[0] - cs->from[0]) / 100;
      cs->dx = (cs->to[1] - cs->from[1]) / 100;
      cs->steps = 0;
      cs->moving = 1;
    }
    else if(cs->done == 1) {
      int newgame = cs->oldgame;
      while(newgame == cs->oldgame)
	newgame = random()%GAMES;

      /* mod the mod */
      cs->mod = 0.6 + (random()%20)/10.0;

      /* same old game */
      cs->oldgame = newgame;
      cs->game = games[cs->oldgame];
      build_colors(cs);
      cs->done = 2;
      cs->count = 0;
    }
    else {
      cs->done = 0;
      cs->count = 0;
    }
  }

  /* set lighting */
  if(cs->done) {
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 
	     cs->done == 1 ? 1.0+0.1*cs->count : 100.0/cs->count);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 
	     cs->done == 1 ? 1.0+0.1*cs->count : 100.0/cs->count);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.14);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.14);
  }

  display(mi, cs);

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

/** bust it */
ENTRYPOINT void release_chess(ModeInfo *mi) 
{
  if(qs)
    free((void *) qs);
  qs = 0;

  FreeAllGL(mi);
}

XSCREENSAVER_MODULE_2 ("Endgame", endgame, chess)

#endif
