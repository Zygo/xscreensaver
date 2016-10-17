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

#ifdef STANDALONE
#define DEFAULTS       "*delay:       20000 \n" \
                       "*showFPS:     False \n" \
		       "*wireframe:   False \n"	\

# define refresh_queens 0
# include "xlockmore.h"

#else
# include "xlock.h"
#endif

#ifdef HAVE_JWXYZ
# include "jwxyz.h"
#else
# include <X11/Xlib.h>
# include <GL/gl.h>
# include <GL/glu.h>
#endif

#ifdef HAVE_JWZGLES
# include "jwzgles.h"
#endif /* HAVE_JWZGLES */

#ifdef USE_GL

#include "gltrackball.h"
#include "chessmodels.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

static XrmOptionDescRec opts[] = {
  {"+rotate", ".queens.rotate", XrmoptionNoArg, "false" },
  {"-rotate", ".queens.rotate", XrmoptionNoArg, "true" },
  {"+flat", ".queens.flat", XrmoptionNoArg, "false" },
  {"-flat", ".queens.flat", XrmoptionNoArg, "true" },
};

static int rotate, wire, clearbits, flat;

static argtype vars[] = {
  {&rotate, "rotate", "Rotate", "True",  t_Bool},
  {&flat,   "flat",   "Flat",   "False", t_Bool},
};

ENTRYPOINT ModeSpecOpt queens_opts = {countof(opts), opts, countof(vars), vars, NULL};

#ifdef USE_MODULES
ModStruct   queens_description =
{"queens", "init_queens", "draw_queens", "release_queens",
 "draw_queens", "init_queens", NULL, &queens_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Queens", 0, NULL};

#endif

#define NONE 0
#define MINBOARD 5
#define MAXBOARD 10
#define COLORSETS 5

typedef struct {
  GLXContext *glx_context;
  Window window;
  trackball_state *trackball;
  Bool button_down_p;
  GLfloat position[4];
  int queen_list;

  int board[MAXBOARD][MAXBOARD];
  int steps, colorset, BOARDSIZE;
  double theta;
  int queen_polys;

} Queenscreen;

static Queenscreen *qss = NULL;

/* definition of white/black colors */
static const GLfloat colors[COLORSETS][2][3] = 
  { 
    {{0.43, 0.54, 0.76}, {0.8, 0.8, 0.8}},
    {{0.5, 0.7, 0.9}, {0.2, 0.3, 0.6}},
    {{0.53725490196, 0.360784313725, 0.521568627451}, {0.6, 0.6, 0.6}},
    {{0.15, 0.77, 0.54}, {0.5, 0.5, 0.5}},
    {{0.9, 0.45, 0.0}, {0.5, 0.5, 0.5}},
  };

ENTRYPOINT Bool
queens_handle_event (ModeInfo *mi, XEvent *event)
{
  Queenscreen *qs = &qss[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, qs->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &qs->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      qs->steps = 1024 - 1;
      return True;
    }

  return False;
}



/* returns true if placing a queen on column c causes a conflict */
static int conflictsCols(Queenscreen *qs, int c) 
{
  int i;

  for(i = 0; i < qs->BOARDSIZE; ++i)
    if(qs->board[i][c])
      return 1;

  return 0;
}

/* returns true if placing a queen on (r,c) causes a diagonal conflict */
static int conflictsDiag(Queenscreen *qs, int r, int c) 
{
  int i;

  /* positive slope */
  int n = r < c ? r : c;
  for(i = 0; i < qs->BOARDSIZE-abs(r-c); ++i)
    if(qs->board[r-n+i][c-n+i])
      return 1;

  /* negative slope */
  n = r < qs->BOARDSIZE - (c+1) ? r : qs->BOARDSIZE - (c+1);
  for(i = 0; i < qs->BOARDSIZE-abs(qs->BOARDSIZE - (1+r+c)); ++i)
    if(qs->board[r-n+i][c+n-i])
      return 1;
  
  return 0;
}

/* returns true if placing a queen at (r,c) causes a conflict */
static int conflicts(Queenscreen *qs, int r, int c) 
{
  return !conflictsCols(qs, c) ? conflictsDiag(qs, r, c) : 1;
}

/* clear board */
static void blank(Queenscreen *qs) 
{
  int i, j;

  for(i = 0; i < MAXBOARD; ++i)
    for(j = 0; j < MAXBOARD; ++j)
      qs->board[i][j] = NONE;
}

/* recursively determine solution */
static int findSolution(Queenscreen *qs, int row, int col) 
{
  if(row == qs->BOARDSIZE)
    return 1;
  
  while(col < qs->BOARDSIZE) {
    if(!conflicts(qs, row, col)) {
      qs->board[row][col] = 1;

      if(findSolution(qs, row+1, 0))
        return 1;

      qs->board[row][col] = 0;
    }

    ++col;
  }

  return 0;
}

/** driver for finding solution */
static void go(Queenscreen *qs) { while(!findSolution(qs, 0, random()%qs->BOARDSIZE)); }

/* lighting variables */
static const GLfloat front_shininess[] = {60.0};
static const GLfloat front_specular[] = {0.4, 0.4, 0.4, 1.0};
static const GLfloat ambient[] = {0.3, 0.3, 0.3, 1.0};
static const GLfloat diffuse[] = {0.8, 0.8, 0.8, 1.0};

/* configure lighting */
static void setup_lights(Queenscreen *qs) 
{

  /* setup twoside lighting */
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, qs->position);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  /* setup material properties */
  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

#define    checkImageWidth 8
#define    checkImageHeight 8
/*static GLubyte checkImage[checkImageWidth][checkImageHeight][3];*/

/* return alpha value for fading */
static GLfloat findAlpha(Queenscreen *qs) 
{
  return qs->steps < 128 ? qs->steps/128.0 : qs->steps < 1024-128 ?1.0:(1024-qs->steps)/128.0;
}

/* draw pieces */
static int drawPieces(Queenscreen *qs) 
{
  int i, j;
  int polys = 0;

  for(i = 0; i < qs->BOARDSIZE; ++i) {
    for(j = 0; j < qs->BOARDSIZE; ++j) {
      if(qs->board[i][j]) {
    	glColor3fv(colors[qs->colorset][i%2]);
	glCallList(qs->queen_list);
        polys += qs->queen_polys;
      }
      
      glTranslatef(1.0, 0.0, 0.0);
    }
    
    glTranslatef(-1.0*qs->BOARDSIZE, 0.0, 1.0);
  }
  return polys;
}

/** reflectionboard */
static int draw_reflections(Queenscreen *qs) 
{
  int i, j;
  int polys = 0;

  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, 1, 1);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
  glColorMask(0,0,0,0);
  glDisable(GL_CULL_FACE);

  glDisable(GL_DEPTH_TEST);
  glBegin(GL_QUADS);

  /* only draw white squares */
  for(i = 0; i < qs->BOARDSIZE; ++i) {
    for(j = (qs->BOARDSIZE+i) % 2; j < qs->BOARDSIZE; j += 2) {
      glVertex3f(i, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j);
      glVertex3f(i, 0.0, j);
      polys++;
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
  glLightfv(GL_LIGHT0, GL_POSITION, qs->position);
  polys += drawPieces(qs);
  glPopMatrix();
  glDisable(GL_STENCIL_TEST);

  /* replace lights */
  glLightfv(GL_LIGHT0, GL_POSITION, qs->position);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glColorMask(1,1,1,1);
  return polys;
}

/* draw board */
static int drawBoard(Queenscreen *qs) 
{
  int i, j;
  int polys = 0;

  glBegin(GL_QUADS);

  for(i = 0; i < qs->BOARDSIZE; ++i)
    for(j = 0; j < qs->BOARDSIZE; ++j) {
      int par = (i-j+qs->BOARDSIZE)%2;
      glColor4f(colors[qs->colorset][par][0],
		colors[qs->colorset][par][1],
		colors[qs->colorset][par][2],
		0.70);
      glNormal3f(0.0, 1.0, 0.0);
      glVertex3f(i, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j + 1.0);
      glVertex3f(i + 1.0, 0.0, j);
      glVertex3f(i, 0.0, j);
      polys++;
    }

  glEnd();

  {
    GLfloat off = 0.01;
    GLfloat w = qs->BOARDSIZE;
    GLfloat h = 0.1;

    /* Give the board a slight lip. */
    /* #### oops, normals are wrong here, but you can't tell */

    glColor3f(0.3, 0.3, 0.3);
    glBegin (GL_QUADS);
    glVertex3f (0,  0, 0);
    glVertex3f (0, -h, 0);
    glVertex3f (0, -h, w);
    glVertex3f (0,  0, w);

    glVertex3f (0,  0, w);
    glVertex3f (0, -h, w);
    glVertex3f (w, -h, w);
    glVertex3f (w,  0, w);

    glVertex3f (w,  0, w);
    glVertex3f (w, -h, w);
    glVertex3f (w, -h, 0);
    glVertex3f (w,  0, 0);

    glVertex3f (w,  0, 0);
    glVertex3f (w, -h, 0);
    glVertex3f (0, -h, 0);
    glVertex3f (0,  0, 0);

    glVertex3f (0, -h, 0);
    glVertex3f (w, -h, 0);
    glVertex3f (w, -h, w);
    glVertex3f (0, -h, w);
    glEnd();
    polys += 4;

    /* Fill in the underside of the board with an invisible black box
       to hide the reflections that are not on tiles.  Probably there's
       a way to do this with stencils instead.
     */
    w -= off*2;
    h = 5;

    glPushMatrix();
    glTranslatef (off, 0, off);
    glDisable(GL_LIGHTING);
    glColor3f(0,0,0);
    glBegin (GL_QUADS);
    glVertex3f (0,  0, 0);
    glVertex3f (0, -h, 0);
    glVertex3f (0, -h, w);
    glVertex3f (0,  0, w);

    glVertex3f (0,  0, w);
    glVertex3f (0, -h, w);
    glVertex3f (w, -h, w);
    glVertex3f (w,  0, w);

    glVertex3f (w,  0, w);
    glVertex3f (w, -h, w);
    glVertex3f (w, -h, 0);
    glVertex3f (w,  0, 0);

    glVertex3f (w,  0, 0);
    glVertex3f (w, -h, 0);
    glVertex3f (0, -h, 0);
    glVertex3f (0,  0, 0);

    glVertex3f (0, -h, 0);
    glVertex3f (w, -h, 0);
    glVertex3f (w, -h, w);
    glVertex3f (0, -h, w);
    glEnd();
    polys += 4;
    glPopMatrix();
    if (!wire)
      glEnable(GL_LIGHTING);
  }

  return polys;
}

static int display(ModeInfo *mi, Queenscreen *qs) 
{
  int max = 1024;
  int polys = 0;
  glClear(clearbits);
  
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glRotatef(current_device_rotation(), 0, 0, 1);

  /* setup light attenuation */
  /* #### apparently this does nothing */
  glEnable(GL_COLOR_MATERIAL);
  glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.8/(0.01+findAlpha(qs)));
  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.06);

  /** setup perspective */
  glTranslatef(0.0, 0.0, -1.5*qs->BOARDSIZE);
  glRotatef(30.0, 1.0, 0.0, 0.0);
  gltrackball_rotate (qs->trackball);
  glRotatef(qs->theta*100, 0.0, 1.0, 0.0);
  glTranslatef(-0.5*qs->BOARDSIZE, 0.0, -0.5*qs->BOARDSIZE);

  /* find light positions */
  qs->position[0] = qs->BOARDSIZE/2.0 + qs->BOARDSIZE/1.4*-sin(qs->theta*100*M_PI/180.0);
  qs->position[2] = qs->BOARDSIZE/2.0 + qs->BOARDSIZE/1.4*cos(qs->theta*100*M_PI/180.0);
  qs->position[1] = 6.0;

  if(!wire) {
    glEnable(GL_LIGHTING);
    glLightfv(GL_LIGHT0, GL_POSITION, qs->position);
    glEnable(GL_LIGHT0);
  }

  /* Since the lighting attenuation trick up there doesn't seem to be working,
     let's drop the old board down and drop the new board in. */
  if (qs->steps < (max/8.0)) {
    GLfloat y = qs->steps / (max/8.0);
    y = sin (M_PI/2 * y);
    glTranslatef (0, 10 - (y * 10), 0);
  } else if (qs->steps > max-(max/8.0)) {
    GLfloat y = (qs->steps - (max-(max/8.0))) / (GLfloat) (max/8.0);
    y = 1 - sin (M_PI/2 * (1-y));
    glTranslatef (0, -y * 15, 0);
  }

  /* draw reflections */
  if(!wire) {
    polys += draw_reflections(qs);
    glEnable(GL_BLEND);
  }
  polys += drawBoard(qs);
  if(!wire)
    glDisable(GL_BLEND);

  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.1);

  glTranslatef(0.5, 0.0, 0.5);
  polys += drawPieces(qs);

  /* rotate camera */
  if(!qs->button_down_p)
    qs->theta += .002;

  /* zero out board, find new solution of size MINBOARD <= i <= MAXBOARD */
  if(++qs->steps == max) {
    qs->steps = 0;
    blank(qs);
    qs->BOARDSIZE = MINBOARD + (random() % (MAXBOARD - MINBOARD + 1));
    qs->colorset = (qs->colorset+1)%COLORSETS;
    go(qs);
  }
  return polys;
}

#define EPSILON 0.001

#if 0
/** draws cylindermodel */
static int draw_model(int chunks, const GLfloat model[][3], int r) 
{
  int i = 0;
  int polys = 0;
  glPushMatrix();
  glRotatef(-90.0, 1.0, 0.0, 0.0);
  
  for(i = 0; i < chunks; ++i) {
    if(model[i][0] > EPSILON || model[i][1] > EPSILON) {
      polys += tube (0, 0, 0,
                     0, 0, model[i][1],
                     model[i][0], 0,
                     r, False, False, False);
/*      gluCylinder(quadric, model[i][0], model[i][1], model[i][2], r, 1);
      polys += r;*/
    }
    glTranslatef(0.0, 0.0, model[i][2]);
  }
  
  glPopMatrix();
  return polys;
}
#endif

ENTRYPOINT void reshape_queens(ModeInfo *mi, int width, int height) 
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  glViewport(0,0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1/h, 2.0, 30.0);
  glMatrixMode(GL_MODELVIEW);
}

ENTRYPOINT void init_queens(ModeInfo *mi) 
{
  int screen = MI_SCREEN(mi);
  Queenscreen *qs;
  int poly_counts[PIECES];
  wire = MI_IS_WIREFRAME(mi);

# ifdef HAVE_JWZGLES /* #### glPolygonMode other than GL_FILL unimplemented */
  wire = 0;
# endif

  if(!qss && 
     !(qss = (Queenscreen *) calloc(MI_NUM_SCREENS(mi), sizeof(Queenscreen))))
    return;
  
  qs = &qss[screen];
  qs->window = MI_WINDOW(mi);
  
  if((qs->glx_context = init_GL(mi)))
    reshape_queens(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  else
    MI_CLEARWINDOW(mi);

  qs->trackball = gltrackball_init (False);

  qs->BOARDSIZE = 8; /* 8 cuz its classic */

  chessmodels_gen_lists(-1, poly_counts);
  qs->queen_list = QUEEN;
  qs->queen_polys = poly_counts[QUEEN];

  /* find a solution */
  go(qs);
}

ENTRYPOINT void draw_queens(ModeInfo *mi) 
{
  Queenscreen *qs = &qss[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if(!qs->glx_context)
    return;

  glXMakeCurrent(disp, w, *(qs->glx_context));

  if(flat)
    glShadeModel(GL_FLAT);
  
  clearbits = GL_COLOR_BUFFER_BIT;

  glColorMaterial(GL_FRONT, GL_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);

  if(!wire) {
    setup_lights(qs);
    glEnable(GL_DEPTH_TEST);
    clearbits |= GL_DEPTH_BUFFER_BIT;
    clearbits |= GL_STENCIL_BUFFER_BIT;
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
  }
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  mi->polygon_count = display(mi, qs);
  mi->recursion_depth = qs->BOARDSIZE;

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

ENTRYPOINT void release_queens(ModeInfo *mi) 
{
  if(qss)
    free((void *) qss);
  qss = 0;

  FreeAllGL(mi);
}

XSCREENSAVER_MODULE ("Queens", queens)

#endif
