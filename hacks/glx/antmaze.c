/*
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
 * Copyright 2004 Blair Tennessy
 * tennessy@cs.ubc.ca
 */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)moebius.c	5.01 2001/03/01 xlockmore";
#endif

#ifdef VMS
#include <X11/Intrinsic.h>
#endif

#ifdef STANDALONE
# define MODE_moebius
# define PROGCLASS			"AntMaze2"
# define HACK_INIT			init_moebius
# define HACK_DRAW			draw_moebius
# define HACK_RESHAPE		reshape_moebius
# define HACK_HANDLE_EVENT	moebius_handle_event
# define EVENT_MASK			PointerMotionMask
# define moebius_opts		xlockmore_opts
# define DEFAULTS			"*delay:		20000   \n"			\
							"*showFPS:      False   \n"			\
							"*wireframe:	False	\n"
# include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"		/* from the xlockmore distribution */

#endif /* !STANDALONE */

#ifdef MODE_moebius


#include <GL/glu.h>
#include "rotator.h"
#include "gltrackball.h"

#define DEF_SOLIDMOEBIUS  "False"
#define DEF_NOANTS  "False"

static int  solidmoebius;
static int  noants;

static XrmOptionDescRec opts[] =
{
  {"-solidmoebius", ".moebius.solidmoebius", XrmoptionNoArg, "on"},
  {"+solidmoebius", ".moebius.solidmoebius", XrmoptionNoArg, "off"},
  {"-noants", ".moebius.noants", XrmoptionNoArg, "on"},
  {"+noants", ".moebius.noants", XrmoptionNoArg, "off"}
};
static argtype vars[] =
{
  {&solidmoebius, "solidmoebius", "Solidmoebius", DEF_SOLIDMOEBIUS, t_Bool},
  {&noants, "noants", "Noants", DEF_NOANTS, t_Bool}
};

static OptionStruct desc[] =
{
	{"-/+solidmoebius", "select between a SOLID or a NET Moebius Strip"},
	{"-/+noants", "turn on/off walking ants"}
};

ModeSpecOpt moebius_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   moebius_description =
{"moebius", "init_moebius", "draw_moebius", "release_moebius",
 "draw_moebius", "change_moebius", NULL, &moebius_opts,
 1000, 1, 1, 1, 4, 1.0, "",
 "draws some ants", 0, NULL};

#endif

#define Scale4Window               0.3
#define Scale4Iconic               0.4

#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi                         M_PI
#endif

#define ObjMoebiusStrip 0
#define ObjAntBody      1
#define MaxObj          2

/*************************************************************************/

typedef struct {
  GLint       WindH, WindW;
  GLfloat     step;
  GLfloat     ant_position;
  GLXContext *glx_context;
  rotator    *rot;
  trackball_state *trackball;
  Bool        button_down_p;
} moebiusstruct;

#include "ants.h"

static GLfloat MaterialRed[] = {0.6, 0.0, 0.0, 1.0};
/*static GLfloat MaterialMagenta[] = {0.6, 0.2, 0.5, 1.0};*/
static GLfloat MaterialGray8[] = {0.8, 0.8, 0.8, 1.0};
static GLfloat MaterialGray35[] = {0.30, 0.30, 0.30, 1.0};
static GLfloat MaterialGray4[] = {0.40, 0.40, 0.40, 1.0};
static GLfloat MaterialOrange[] = {1.0, 0.69, 0.00, 1.0};
static GLfloat MaterialGreen[] = {0.1, 0.4, 0.2, 1.0};

/* lighting variables */
GLfloat front_shininess[] = {60.0};
GLfloat front_specular[] = {0.8, 0.8, 0.8, 1.0};
GLfloat ambient[] = {0.1, 0.1, 0.1, 1.0};
GLfloat ambient2[] = {0.0, 0.0, 0.0, 0.0};
GLfloat diffuse[] = {0.8, 0.8, 0.8, 1.0};
GLfloat position0[] = {1.0, 5.0, 1.0, 1.0};
GLfloat position1[] = {-1.0, -5.0, 1.0, 1.0};
GLfloat lmodel_ambient[] = {0.8, 0.8, 0.8, 1.0};
GLfloat lmodel_twoside[] = {GL_TRUE};
GLfloat spotlight_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
GLfloat spotlight_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };

static moebiusstruct *moebius = (moebiusstruct *) NULL;

#define NUM_SCENES      2

/* filled sphere */
static Bool mySphere(float radius) {
  GLUquadricObj *quadObj;

  if((quadObj = gluNewQuadric()) == 0)
	return False;
  gluQuadricDrawStyle(quadObj, (GLenum) GLU_FILL);
  gluSphere(quadObj, radius, 16, 16);
  gluDeleteQuadric(quadObj);

  return True;
}

#if 0
/* silhouette sphere */
static Bool mySphere2(float radius) {
  GLUquadricObj *quadObj;

  if((quadObj = gluNewQuadric()) == 0)
	return False;
  gluQuadricDrawStyle(quadObj, (GLenum) GLU_SILHOUETTE);
  gluSphere(quadObj, radius, 16, 8);
  gluDeleteQuadric(quadObj);

  return True;
}
#endif

/* textured sphere */
static Bool mySphereTex(float radius) {
  GLUquadricObj *quadObj;
  
  if((quadObj = gluNewQuadric()) == 0)
	return False;
  gluQuadricDrawStyle(quadObj, (GLenum) GLU_FILL);
  gluQuadricTexture(quadObj, GL_TRUE);
  gluQuadricNormals(quadObj, GLU_SMOOTH);
  gluSphere(quadObj, radius, 32, 16);
  gluDeleteQuadric(quadObj);

  return True;
}

/* filled cone */
static Bool myCone(float radius) {
  GLUquadricObj *quadObj;
  
  if ((quadObj = gluNewQuadric()) == 0)
    return False;
  gluQuadricDrawStyle(quadObj, (GLenum) GLU_FILL);
  gluCylinder(quadObj, radius, 0, radius * 2, 8, 1);
  gluDeleteQuadric(quadObj);
  return True;
}

/* no cone */
static Bool myCone2(float radius) { return True; }

#define ANTCOUNT 5
#define PI 3.14157

static int focus = 0;

#define MATERIALS 4
static float* antmaterial[ANTCOUNT] = 
  {MaterialRed, MaterialGray35, MaterialGray4, MaterialOrange, MaterialGreen};

static float* materials[MATERIALS] = 
  {MaterialRed, MaterialGray35, MaterialGray4, MaterialOrange};

static double antdirection[ANTCOUNT] = {PI/2.0, PI/2.0, 0.0, PI/2.0, PI/2.0};

static double antposition[ANTCOUNT][3] = {{-4.0, 5.0, 0.15},
					  {-4.0, 3.0, 0.15},
					  {-1.0, -2.0, 0.15},
					  {-3.9, 6.0, 0.15}, 
					  {2.0, -2.0, 0.15}};

static int anton[ANTCOUNT] = {1, 0, 0, 0};

static double antvelocity[ANTCOUNT] = {0.02,
				       0.02,
				       0.02,
				       0.02};

static double antsize[ANTCOUNT] = {1.0,
				   1.0,
				   1.0,
				   1.0};

#define EPSILON 0.01
#define BOARDSIZE 10

int bposition[ANTCOUNT][2] = {{0, 8},
			      {9, 1},
			      {1, 1},
			      {4, 8},
			      {2, 1},};

#define BOARDCOUNT 2
int currentboard = 0;

int board[BOARDCOUNT][10][10];/*  = {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, */
/* 		     1, 0, 1, 0, 1, 0, 1, 1, 0, 1, */
/* 		     1, 0, 0, 0, 1, 0, 0, 0, 0, 1, */
/* 		     1, 0, 1, 1, 0, 0, 1, 1, 0, 1, */
/* 		     1, 0, 1, 1, 0, 1, 1, 0, 0, 1, */
/* 		     1, 0, 1, 0, 0, 1, 0, 0, 1, 1, */
/* 		     1, 0, 0, 0, 1, 1, 0, 1, 0, 1, */
/* 		     1, 0, 1, 0, 1, 1, 0, 0, 0, 1, */
/* 		     1, 0, 1, 0, 0, 0, 1, 0, 1, 1, */
/* 		     1, 0, 1, 1, 1, 1, 1, 1, 1, 1,}; */

#define PARTS 20
int part[ANTCOUNT] = {0, 1, 5, 1, 3};
double antpath[ANTCOUNT][PARTS][2];/*  = {{-4.0, 5.0}, */
int antpathlength[ANTCOUNT];
/* 				      {-4.0, 1.0}, */
/* 				      {-2.0, 1.0}, */
/* 				      {-2.0, 0.0}, */
/* 				      {-1.0, 0.0}, */
/* 				      {-1.0, -2.0}, */
/* 				      { 0.0, -2.0}, */
/* 				      { 0.0, -3.0}, */
/* 				      { 3.0, -3.0}, */
/* 				      { 3.0, -6.0},}; */

#define checkImageWidth 64
#define checkImageHeight 64
GLubyte checkers[checkImageWidth][checkImageHeight][3];
GLuint checktexture, brushedtexture;
double elevator = 0.0;

void makeCheckImage(void) {
  int i, j;
  
  for (i = 0; i < checkImageWidth; i++) {
    for (j = 0; j < checkImageHeight; j++) {
      if(((((i&0x8)==0)^((j&0x8)))==0)) {
	int c = 102 + random()%32;
	checkers[i][j][0] = c;
	checkers[i][j][1] = c;
	checkers[i][j][2] = c;
      }
      else {
	int c = 153 + random()%32;
	checkers[i][j][0] = c;/*153;*/
	checkers[i][j][1] = c;/*c;*//*0;*/
	checkers[i][j][2] = c;/*c;*//*0;*/
      }
    }
  }

  glGenTextures(1, &checktexture);
  glBindTexture(GL_TEXTURE_2D, checktexture);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, 3, checkImageWidth, 
	       checkImageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 
	       &checkers[0][0]);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void makeBrushedImage(void) {
  int i, j, c;

  for(i = 0; i < checkImageWidth; ++i)
    for(j = 0; j < checkImageHeight; ++j) {

      c = 102+102*fabs(sin(2.0*i / Pi)*sin(2.0*j/Pi)) + random()%51;

/*       c = (i+j)%8==0 || (i+j+5)%8==0 ? 153 : 102; */

      checkers[i][j][0] = c;
      checkers[i][j][1] = c;
      checkers[i][j][2] = c;
    }
  
/*   for (i = 0; i < checkImageWidth; i++) { */
/*     for (j = 0; j < checkImageHeight; j++) { */
/*       int c = 102 + pow((random()%1000)/1000.0, 4)*103; */
/*       checkers[i][j][0] = c; */
/*       checkers[i][j][1] = c; */
/*       checkers[i][j][2] = c; */
/*     } */
/*   } */

/*   /\* smooth *\/ */
/*   for (i = 0; i < checkImageWidth; i++) { */
/*     for (j = 0; j < checkImageHeight; j++) { */
/*       int a = checkers[(i+checkImageWidth+1)%checkImageWidth][j][0] +  */
/* 	4*checkers[i][j][0] + checkers[(i+1)%checkImageWidth][j][0]; */
/*       a /= 6; */
/*       checkers[i][j][0] = a; */
/*       checkers[i][j][1] = a; */
/*       checkers[i][j][2] = a; */
/*     } */
/*   } */

  glGenTextures(1, &brushedtexture);
  glBindTexture(GL_TEXTURE_2D, brushedtexture);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, 3, checkImageWidth, 
	       checkImageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 
	       &checkers[0][0]);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void draw_wall(double x1, double z1, double x2, double z2) {
  float x = fabs(x2 - x1)/2.0;

  glBegin(GL_QUADS);

  /* draw top */
  glNormal3f(0.0, 1.0, 0.0);
  glTexCoord2f(0.0, 0.0);
  glVertex3f(x1, 1.0, z1+0.25);
  glTexCoord2f(x, 0.0);
  glVertex3f(x2, 1.0, z2+0.25);
  glTexCoord2f(x, 0.25);
  glVertex3f(x2, 1.0, z2-0.25);
  glTexCoord2f(0.0, 0.25);
  glVertex3f(x1, 1.0, z1-0.25);

  /* draw sides */
  glNormal3f(0.0, 0.0, 1.0);
  glTexCoord2f(0.0, 0.0);
  glVertex3f(x1, 0.0, z1+0.25);
  glTexCoord2f(x, 0.0);
  glVertex3f(x2, 0.0, z2+0.25);
  glTexCoord2f(x, 0.5);
  glVertex3f(x2, 1.0, z2+0.25);
  glTexCoord2f(0.0, 0.5);
  glVertex3f(x1, 1.0, z1+0.25);

  glNormal3f(0.0, 0.0, -1.0);
  glTexCoord2f(0.0, 0.0);
  glVertex3f(x1, 0.0, z1-0.25);
  glTexCoord2f(x, 0.0);
  glVertex3f(x2, 0.0, z2-0.25);
  glTexCoord2f(x, 0.5);
  glVertex3f(x2, 1.0, z2-0.25);
  glTexCoord2f(0.0, 0.5);
  glVertex3f(x1, 1.0, z1-0.25);

  /* draw ends */
  glNormal3f(1.0, 0.0, 0.0);
  glTexCoord2f(0.0, 0.0);
  glVertex3f(x2, 0.0, z2+0.25);
  glTexCoord2f(0.25, 0.0);
  glVertex3f(x2, 0.0, z2-0.25);
  glTexCoord2f(0.25, 0.5);
  glVertex3f(x2, 1.0, z2-0.25);
  glTexCoord2f(0.0, 0.5);
  glVertex3f(x2, 1.0, z2+0.25);

  glNormal3f(-1.0, 0.0, 0.0);
  glTexCoord2f(0.0, 0.0);
  glVertex3f(x1, 0.0, z1-0.25);
  glTexCoord2f(0.25, 0.0);
  glVertex3f(x1, 0.0, z1+0.25);
  glTexCoord2f(0.25, 0.5);
  glVertex3f(x1, 1.0, z1+0.25);
  glTexCoord2f(0.0, 0.5);
  glVertex3f(x1, 1.0, z1-0.25);

  glEnd();
}

int pastfirst = 0;

void draw_board(void) {

  int i, j;
  double h = 0.5;
  double stf = 0.0625;

  glBindTexture(GL_TEXTURE_2D, checktexture);

  glBegin(GL_QUADS);

  for(i = 0; i < BOARDSIZE; ++i)
    for(j = 0; j < BOARDSIZE; ++j) {
      if(board[currentboard][j][i]) {

/* 	/\* draw top *\/ */
/* 	glNormal3f(0.0, 1.0, 0.0); */
/* 	glTexCoord2f(0.0 + stf, 0.0 + stf); */
/* 	glVertex3f(i-0.5, h, j+0.5); */
/* 	glTexCoord2f(1.0 + stf, 0.0 + stf); */
/* 	glVertex3f(i+0.5, h, j+0.5); */
/* 	glTexCoord2f(1.0 + stf, 1.0 + stf); */
/* 	glVertex3f(i+0.5, h, j-0.5); */
/* 	glTexCoord2f(0.0 + stf, 1.0 + stf); */
/* 	glVertex3f(i-0.5, h, j-0.5); */

	/* draw top */
	glNormal3f(0.0, 1.0, 0.0);
	glTexCoord2f(0.0 + stf, 0.0 + stf);
	glVertex3f(i-0.5, h, j+0.5);
	glTexCoord2f(1.0 + stf, 0.0 + stf);
	glVertex3f(i+0.5, h, j+0.5);
	glTexCoord2f(1.0 + stf, 1.0 + stf);
	glVertex3f(i+0.5, h, j-0.5);
	glTexCoord2f(0.0 + stf, 1.0 + stf);
	glVertex3f(i-0.5, h, j-0.5);

	/* draw south face */
	if(j == 9 || !board[currentboard][j+1][i]) {
	  glNormal3f(0.0, 0.0, 1.0);
	  glTexCoord2f(0.0 + stf, 0.0 + stf);
	  glVertex3f(i-0.5, 0.0, j+0.5);
	  glTexCoord2f(1.0 + stf, 0.0 + stf);
	  glVertex3f(i+0.5, 0.0, j+0.5);
	  glTexCoord2f(1.0 + stf, h + stf);
	  glVertex3f(i+0.5, h, j+0.5);
	  glTexCoord2f(0.0 + stf, h + stf);
	  glVertex3f(i-0.5, h, j+0.5);
	}

	/* draw north face */
	if(j == 0 || !board[currentboard][j-1][i]) {
	  glNormal3f(0.0, 0.0, -1.0);
	  glTexCoord2f(0.0 + stf, 0.0 + stf);
	  glVertex3f(i+0.5, 0.0, j-0.5);
	  glTexCoord2f(1.0 + stf, 0.0 + stf);
	  glVertex3f(i-0.5, 0.0, j-0.5);
	  glTexCoord2f(1.0 + stf, h + stf);
	  glVertex3f(i-0.5, h, j-0.5);
	  glTexCoord2f(0.0 + stf, h + stf);
	  glVertex3f(i+0.5, h, j-0.5);
	}

	/* draw east face */
	if(i == 9 || !board[currentboard][j][i+1]) {
	  glNormal3f(1.0, 0.0, 0.0);
	  glTexCoord2f(0.0 + stf, 0.0 + stf);
	  glVertex3f(i+0.5, 0.0, j+0.5);
	  glTexCoord2f(1.0 + stf, 0.0 + stf);
	  glVertex3f(i+0.5, 0.0, j-0.5);
	  glTexCoord2f(1.0 + stf, h + stf);
	  glVertex3f(i+0.5, h, j-0.5);
	  glTexCoord2f(0.0 + stf, h + stf);
	  glVertex3f(i+0.5, h, j+0.5);
	}

	/* draw west face */
	if(i == 0 || !board[currentboard][j][i-1]) {
	  glNormal3f(-1.0, 0.0, 0.0);
	  glTexCoord2f(0.0 + stf, 0.0 + stf);
	  glVertex3f(i-0.5, 0.0, j-0.5);
	  glTexCoord2f(1.0 + stf, 0.0 + stf);
	  glVertex3f(i-0.5, 0.0, j+0.5);
	  glTexCoord2f(1.0 + stf, h + stf);
	  glVertex3f(i-0.5, h, j+0.5);
	  glTexCoord2f(0.0 + stf, h + stf);
	  glVertex3f(i-0.5, h, j-0.5);
	}
      }
      else {
	double tx = 2.0;
	glNormal3f(0.0, 1.0, 0.0);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(i-0.5, 0.0, j+0.5);
	glTexCoord2f(tx, 0.0);
	glVertex3f(i+0.5, 0.0, j+0.5);
	glTexCoord2f(tx, tx);
	glVertex3f(i+0.5, 0.0, j-0.5);
	glTexCoord2f(0.0, tx);
	glVertex3f(i-0.5, 0.0, j-0.5);
      }
    }
  glEnd();

/*   /\* draw elevator *\/ */
/*   glBindTexture(GL_TEXTURE_2D, brushedtexture); */

/*   glBegin(GL_QUADS); */

/*   glNormal3f(0.0, 1.0, 0.0); */

/*   if(pastfirst) { */
/*       /\* source *\/ */
/*       glTexCoord2f(0.0, 0.0); */
/*       glVertex3f(0.5, 0.0, BOARDSIZE - 0.5 + 0.2); */
/*       glTexCoord2f(1.0, 0.0); */
/*       glVertex3f(1.5, 0.0, BOARDSIZE - 0.5 + 0.2); */
/*       glTexCoord2f(1.0, 1.5); */
/*       glVertex3f(1.5, 0.0, BOARDSIZE + 1.0 + 0.2); */
/*       glTexCoord2f(0.0, 1.5); */
/*       glVertex3f(0.5, 0.0, BOARDSIZE + 1.0 + 0.2); */
/*   } */

/*   /\* destination *\/ */
/*   glTexCoord2f(0.0, 0.0); */
/*   glVertex3f(BOARDSIZE - 2.5, elevator, -2.0 - 0.2); */
/*   glTexCoord2f(1.0, 0.0); */
/*   glVertex3f(BOARDSIZE - 1.5, elevator, -2.0 - 0.2); */
/*   glTexCoord2f(1.0, 1.5); */
/*   glVertex3f(BOARDSIZE - 1.5, elevator, -0.5 - 0.2); */
/*   glTexCoord2f(0.0, 1.5); */
/*   glVertex3f(BOARDSIZE - 2.5, elevator, -0.5 - 0.2); */

/*   glEnd(); */

/*   for(i = 0; i < BOARDSIZE; ++i) */
/*     for(j = 0; j < BOARDSIZE; ++j) { */
/*       if(board[j][i]) { */

/* 	/\* draw brushed boxtop *\/ */
/* 	glNormal3f(0.0, 1.0, 0.0); */
/* 	glTexCoord2f(0.0 + stf, 0.0 + stf); */
/* 	glVertex3f(i-0.5 + stf, h+0.001, j+0.5 - stf); */
/* 	glTexCoord2f(1.0 + stf, 0.0 + stf); */
/* 	glVertex3f(i+0.5 - stf, h+0.001, j+0.5 - stf); */
/* 	glTexCoord2f(1.0 + stf, 1.0 + stf); */
/* 	glVertex3f(i+0.5 - stf, h+0.001, j-0.5 + stf); */
/* 	glTexCoord2f(0.0 + stf, 1.0 + stf); */
/* 	glVertex3f(i-0.5 + stf, h+0.001, j-0.5 + stf); */
/*       } */
/*     } */
  
/*   glEnd(); */
}

void build_board(int b) {
  int i, j;

  for(i = 0; i < BOARDSIZE; ++i)
    for(j = 0; j < BOARDSIZE; ++j)
      board[b][i][j] = 1;
  
/*   for(i = 0; i < BOARDSIZE; ++i) { */
/*     board[0][i] = 1; */
/*     board[i][0] = 1; */
/*     board[BOARDSIZE-1][BOARDSIZE-i] = 1; */
/*     board[BOARDSIZE-i][BOARDSIZE-1] = 1; */
/*   } */

/*   board[0][BOARDSIZE-2] = 0; */
/*   board[BOARDSIZE-1][1] = 0; */


  board[b][BOARDSIZE-1][1] = 0;
  board[b][0][BOARDSIZE-2] = 0;

  /* build the ant paths */
  if(currentboard == b) {
    for(i = 0; i < ANTCOUNT; ++i) {
      int sx = BOARDSIZE-2;
      int sy = 1;
      
      for(j = 0; ; ++j) {
	board[b][sx][sy] = 0;
	antpath[i][j][0] = sy - 5.0;
	antpath[i][j][1] = sx - 5.0;
	
	if(random()%2) {
	  if(sx > 1)
	    sx -= 1;
	  else if(sy < BOARDSIZE-2)
	    sy += 1;
	  else
	    break;
	}
	else {
	  if(sy < BOARDSIZE-2)
	    sy += 1;
	  else if(sx > 1)
	    sx -= 1;
	  else
	    break;
	}
      }
      
      ++j;
      antpath[i][j][0] = BOARDSIZE-7.0;
      antpath[i][j][1] = -7.0;
      antpathlength[i] = j;
    }
  }

/*   for(i = 0; i < 20; ++i) { */
/*     int x = 1 + random()%(BOARDSIZE-2); */
/*     int y = 1 + random()%(BOARDSIZE-2); */
/*     board[x][y] = 1; */
/*   } */
}

/* compute nearness */
int near(double a[2], double b[2]) {
  return fabs(a[0] - b[0]) < 0.5 && fabs(a[1] - b[1]) < 0.5;
}

double sign(double d) {
  return d < 0.0 ? -1.0 : 1.0;
}

double min(double a, double b) {
  return a < b ? a : b;
}

/* draw method for ant */
Bool draw_ant(float *Material, int mono, int shadow,
	      float ant_step, Bool (*sphere)(float), Bool (*cone)(float)) {
  
  float cos1 = cos(ant_step);
  float cos2 = cos(ant_step + 2 * Pi / 3);
  float cos3 = cos(ant_step + 4 * Pi / 3);
  float sin1 = sin(ant_step);
  float sin2 = sin(ant_step + 2 * Pi / 3);
  float sin3 = sin(ant_step + 4 * Pi / 3);
  
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mono ? MaterialGray5 : Material);

/*   glEnable(GL_CULL_FACE); */

  glPushMatrix();
  glScalef(1, 1.3, 1);
  if(!((*sphere)(0.18)))
    return False;
  glScalef(1, 1 / 1.3, 1);
  glTranslatef(0.00, 0.30, 0.00);
  if(!((*sphere)(0.2)))
    return False;
  
  glTranslatef(-0.05, 0.17, 0.05);
  glRotatef(-90, 1, 0, 0);
  glRotatef(-25, 0, 1, 0);
  if(!((*cone)(0.05)))
    return False;
  glTranslatef(0.00, 0.10, 0.00);
  if(!((*cone)(0.05)))
    return False;
  glRotatef(25, 0, 1, 0);
  glRotatef(90, 1, 0, 0);
  
  glScalef(1, 1.3, 1);
  glTranslatef(0.15, -0.65, 0.05);
  if(!((*sphere)(0.25)))
    return False;
  glScalef(1, 1 / 1.3, 1);
  glPopMatrix();

/*   glDisable(GL_CULL_FACE); */
  
  glDisable(GL_LIGHTING);

  /* ANTENNAS */
  glBegin(GL_LINES);
  glColor3fv(mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.30, 0.00);
  glColor3fv(MaterialGray);
  glVertex3f(0.40, 0.70, 0.40);
  glColor3fv(mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.30, 0.00);
  glColor3fv(MaterialGray);
  glVertex3f(0.40, 0.70, -0.40);
  glEnd();

  if(!shadow) {
    glBegin(GL_POINTS);
    glColor3fv(mono ? MaterialGray6 : MaterialRed);
    glVertex3f(0.40, 0.70, 0.40);
    glVertex3f(0.40, 0.70, -0.40);
    glEnd();
  }

  /* LEFT-FRONT ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.05, 0.18);
  glVertex3f(0.35 + 0.05 * cos1, 0.15, 0.25);
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 + 0.05 * cos1, 0.25 + 0.1 * sin1, 0.45);
  glEnd();

  /* LEFT-CENTER ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.00, 0.18);
  glVertex3f(0.35 + 0.05 * cos2, 0.00, 0.25);
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 + 0.05 * cos2, 0.00 + 0.1 * sin2, 0.45);
  glEnd();

  /* LEFT-BACK ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mono ? MaterialGray5 : Material);
  glVertex3f(0.00, -0.05, 0.18);
  glVertex3f(0.35 + 0.05 * cos3, -0.15, 0.25);
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 + 0.05 * cos3, -0.25 + 0.1 * sin3, 0.45);
  glEnd();

  /* RIGHT-FRONT ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.05, -0.18);
  glVertex3f(0.35 - 0.05 * sin1, 0.15, -0.25);
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 - 0.05 * sin1, 0.25 + 0.1 * cos1, -0.45);
  glEnd();

  /* RIGHT-CENTER ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.00, -0.18);
  glVertex3f(0.35 - 0.05 * sin2, 0.00, -0.25);
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 - 0.05 * sin2, 0.00 + 0.1 * cos2, -0.45);
  glEnd();

  /* RIGHT-BACK ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mono ? MaterialGray5 : Material);
  glVertex3f(0.00, -0.05, -0.18);
  glVertex3f(0.35 - 0.05 * sin3, -0.15, -0.25);
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 - 0.05 * sin3, -0.25 + 0.1 * cos3, -0.45);
  glEnd();

  if(!shadow) {
    glBegin(GL_POINTS);
    glColor3fv(mono ? MaterialGray8 : MaterialGray35);
    glVertex3f(-0.20 + 0.05 * cos1, 0.25 + 0.1 * sin1, 0.45);
    glVertex3f(-0.20 + 0.05 * cos2, 0.00 + 0.1 * sin2, 0.45);
    glVertex3f(-0.20 + 0.05 * cos3, -0.25 + 0.1 * sin3, 0.45);
    glVertex3f(-0.20 - 0.05 * sin1, 0.25 + 0.1 * cos1, -0.45);
    glVertex3f(-0.20 - 0.05 * sin2, 0.00 + 0.1 * cos2, -0.45);
    glVertex3f(-0.20 - 0.05 * sin3, -0.25 + 0.1 * cos3, -0.45);
    glEnd();
  }

  glEnable(GL_LIGHTING);

  return True;
}

double ant_step = 0.0;
double first_ant_step = 0.0;
int started = 0;
int introduced = 300;
int fir = 0;
int makenew = 1;
int entroducing = 12;

static Bool draw_moebius_strip(ModeInfo * mi) {
  moebiusstruct *mp = &moebius[MI_SCREEN(mi)];
  int i;
  int mono = MI_IS_MONO(mi);

/*   glMatrixMode(GL_MODELVIEW); */
/*   glLoadIdentity(); */
/*   glPushMatrix(); */

  glEnable(GL_LIGHTING);
/*   glDisable(GL_BLEND); */
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);

  /* set light */
/*   double l1 = 1.0 - (elevator < 1.0 ? elevator : 2.0 - elevator); */
/*   GLfloat df[4] = {0.8*l1, 0.8*l1, 0.8*l1, 1.0}; */
/*   glLightfv(GL_LIGHT0, GL_DIFFUSE, df); */
/*   glLightfv(GL_LIGHT1, GL_DIFFUSE, df); */

  /* draw board */
  if(elevator < 1.0) {
    glEnable(GL_TEXTURE_2D);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray6);
    glTranslatef(-(BOARDSIZE-1)/2.0, 0.0, -(BOARDSIZE-1)/2.0);
    draw_board();
    glTranslatef(BOARDSIZE/2.0, 0.0, BOARDSIZE/2.0);
    glDisable(GL_TEXTURE_2D);
  }

  introduced--;

  glTranslatef(0.0, -0.1, 0.0);

  for(i = 0; i < ANTCOUNT; ++i) {

/*     glLightfv(GL_LIGHT0, GL_DIFFUSE, df); */
/*     glLightfv(GL_LIGHT1, GL_DIFFUSE, df); */

    if(!anton[i]) { continue; }

    /* determine location, move to goal */
    glPushMatrix();
    glTranslatef(0.0, 0.01, 0.0);
    glTranslatef(antposition[i][0], antposition[i][2], antposition[i][1]);
/*     glScalef(1.0, 0.01, 1.0); */
    glScalef(0.6, 0.01, 0.6);
    glRotatef(180.0 + antdirection[i]*180.0/PI, 0.0, 1.0, 0.0);
    glRotatef(90.0, 0.0, 0.0, 1.0);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4fv(MaterialGrayB);

    glScalef(antsize[i], antsize[i], antsize[i]);

    /* slow down first ant */
    if(i == 0 && part[i] == antpathlength[i])
      draw_ant(MaterialGrayB, mono, 1, first_ant_step, mySphere, myCone);
    else
      draw_ant(MaterialGrayB, mono, 1, ant_step, mySphere, myCone);

    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    
    glPushMatrix();
/*     glTranslatef(0.0, 0.18, 0.0); */
    glTranslatef(0.0, 0.12, 0.0);
    glTranslatef(antposition[i][0], antposition[i][2], antposition[i][1]);
    glRotatef(180.0 + antdirection[i]*180.0/PI, 0.0, 1.0, 0.0);
    glRotatef(90.0, 0.0, 0.0, 1.0);
    glScalef(0.6, 0.6, 0.6);

    glScalef(antsize[i], antsize[i], antsize[i]);

/*     glEnable(GL_TEXTURE_2D); */
/*     glBindTexture(GL_TEXTURE_2D, brushedtexture); */

/*     glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialRed); */

    /* slow down first ant */    
    if(i == 0 && part[i] == antpathlength[i] && elevator > 0.0) {
      glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
      glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
      draw_ant(antmaterial[i], mono, 1, first_ant_step, mySphere, myCone);
    }
    else {
/*       glLightfv(GL_LIGHT0, GL_DIFFUSE, df); */
/*       glLightfv(GL_LIGHT1, GL_DIFFUSE, df); */

      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, brushedtexture);
      draw_ant(antmaterial[i], mono, 1, ant_step, mySphereTex, myCone);
      glDisable(GL_TEXTURE_2D);
    }


/*     draw_ant(antmaterial[i], mono, 0, ant_step, mySphereTex, myCone); */
/*     glDisable(GL_TEXTURE_2D); */
    glPopMatrix();
  }

/*   glPopMatrix(); */

/*   /\* now draw overlay *\/ */
/*   glDisable(GL_LIGHTING); */
/*   glDisable(GL_BLEND); */

/*   /\* go to ortho mode *\/ */
/*   glMatrixMode(GL_PROJECTION); */
/*   glPushMatrix(); */
/*   glLoadIdentity(); */
/*   glOrtho(-4.0, 4.0, -3.0, 3.0, -100.0, 100.0); */
  
/*   /\* translate to corner *\/ */
/*   glTranslatef(4.0-1.2, 3.0-1.2, 0.0); */

/*   glDisable(GL_LIGHTING); */
/*   glEnable(GL_BLEND); */

/*   /\* draw the 2d board *\/ */
/*   glBegin(GL_QUADS); */
/*   { */
/*     int i, j; */
/*     double sz = 1.0; */
/*     for(i = 0; i < BOARDSIZE; ++i) */
/*       for(j = 0; j < BOARDSIZE; ++j) { */
/* 	int par = board[i][j]; */
/* 	glColor4f(par ? 0.4 : 0.6, */
/* 		  par ? 0.4 : 0.6, */
/* 		  par ? 0.4 : 0.6, */
/* 		  0.5); */
/* 	glNormal3f(0.0, 0.0, 1.0); */
/* 	glVertex3f((sz*(i+1))/BOARDSIZE,     (sz*(j+1))/BOARDSIZE, 0.0); */
/* 	glVertex3f((sz*i)/BOARDSIZE, (sz*(j+1))/BOARDSIZE, 0.0); */
/* 	glVertex3f((sz*i)/BOARDSIZE, (sz*j)/BOARDSIZE, 0.0); */
/* 	glVertex3f((sz*(i+1))/BOARDSIZE, (sz*j)/BOARDSIZE, 0.0); */
/*       } */
/*   } */
/*   glEnd(); */
  
/*   glPopMatrix(); */


  /* but the step size is the same! */
  ant_step += 0.18;
/*   if(ant_step > 2*Pi) { */
/*     ant_step = 0.0; */
/*   } */

  if(ant_step > 5*Pi)
    started = 1;

  mp->ant_position += 1;
  return True;
}
#undef MoebiusDivisions
#undef MoebiusTransversals

void reshape_moebius(ModeInfo * mi, int width, int height) {
  double h = (GLfloat) height / (GLfloat) width;  
  int size = (width / 512) + 1;
  moebiusstruct *mp = &moebius[MI_SCREEN(mi)];

  glViewport(0, 0, mp->WindW = (GLint) width, mp->WindH = (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective(45, 1/h, 0.001, 25.0);

  glMatrixMode(GL_MODELVIEW);
/*   glLineWidth(3.0); */
   glLineWidth(size);
  glPointSize(size);
}

double fadeout = 1.0;
double fadeoutspeed = 0.0;

void update_ants(void) {
  int i;
  GLfloat df[4];
  df[0] = df[1] = df[2] = 0.8*fadeout;
  df[3] = 1.0;

  /* fade out */
  if(fadeoutspeed < -0.00001) {

    if(fadeout <= 0.0) {
      /* switch boards: rebuild old board, increment current */
      currentboard = (currentboard+1)%BOARDCOUNT;
      build_board(currentboard);
      fadeoutspeed = 0.02;
    }
    
    fadeout += fadeoutspeed;

    glLightfv(GL_LIGHT0, GL_DIFFUSE, df);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, df);
  }

  /* fade in */
  if(fadeoutspeed > 0.0001) {
    fadeout += fadeoutspeed;
    if(fadeout >= 1.0) {
      fadeout = 1.0;
      fadeoutspeed = 0.0;
      entroducing = 12;
    }
    glLightfv(GL_LIGHT0, GL_DIFFUSE, df);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, df);    
  }

  for(i = 0; i < ANTCOUNT; ++i) {

    if(!anton[i] && elevator < 1.0) {

      /* turn on ant */
      if(entroducing > 0 && introduced <= 0 && random()%100 == 0) {
	anton[i] = 1;
	part[i] = 0;
	antsize[i] = 0.0;
	antposition[i][0] = -4.0;
	antposition[i][1] = 5.0;
	antdirection[i] = PI/2.0;
	bposition[i][0] = 0;
	bposition[i][1] = 8;
	introduced = 300;
	entroducing--;
      }

      continue;
    }

    if(part[i] == 0 && antsize[i] < 1.0) {
      antsize[i] += 0.02;
      continue;
    }

    if(part[i] > antpathlength[i] && antsize[i] > 0.0) {
      antsize[i] -= 0.02;
      if(antvelocity[i] > 0.0) {
	antvelocity[i] -= 0.02;
      }
      else { antvelocity[i] = 0.0; }

      continue;
    }

    if(part[i] > antpathlength[i] && antsize[i] <= 0.0) {
      antvelocity[i] = 0.02;
      
      /* 	if(i != 0) { */
      antmaterial[i] = materials[random()%MATERIALS];
      /* 	} */
      
      antdirection[i] = PI/2.0;
      bposition[i][0] = 0;
      bposition[i][1] = 8;
      part[i] = 0;
      
      antsize[i] = 0.0;
      
      anton[i] = 0;
      
      antposition[i][0] = -4.0;
      antposition[i][1] = 5.0;
      
      /* 	/\* reset camera *\/ */
      /* 	if(i == focus) { */
      /* 	  started = 0; */
      /* 	  ant_step = 0.0; */
      /* 	} */
      
      /* check for the end */
      if(entroducing <= 0) {
	int ao = 0, z = 0;
	for(z = 0; z < ANTCOUNT; ++z) {
	  if(anton[z]) { ao = 1; break; }
	}

	if(ao == 0) {
	  fadeoutspeed = -0.02;
	}
      }

    }
    
    /* near goal, bend path towards next step */
    if(near(antposition[i], antpath[i][part[i]])) {
      
      ++part[i];

/*       /\* special first ant *\/ */
/*       if(i == 0 && part[i] > antpathlength[i]) { */
/* 	if(fir) */
/* 	  first_ant_step = ant_step; */

/* 	antvelocity[i] = 0.0; */
/* /\* 	antposition[i][2] += 0.025; *\/ */
/* 	elevator += 0.025; */

/* 	/\* set light *\/ */
/* 	double l1 = 1.0 - (elevator < 1.0 ? elevator : 2.0 - elevator); */
/* 	GLfloat df[4] = {0.8*l1, 0.8*l1, 0.8*l1, 1.0}; */
/* 	glLightfv(GL_LIGHT0, GL_DIFFUSE, df); */
/* 	glLightfv(GL_LIGHT1, GL_DIFFUSE, df); */

/* 	/\* draw next board *\/ */
/* 	if(elevator > 1.0) { */

/* 	  if(makenew == 1) { */
/* 	    int re; */
	  
/* 	    /\* switch boards: rebuild old board, increment current *\/ */
/* 	    currentboard = (currentboard+1)%BOARDCOUNT; */
/* 	    build_board(currentboard); */

/* 	    for(re = 1; re < ANTCOUNT; ++re) { */
/* 	      anton[re] = 0; */
/* 	      antmaterial[re] = materials[random()%MATERIALS]; */
/* 	    } */

/* 	    makenew = 0; */

/* 	  } */

/* 	  /\* draw the other board *\/ */
/* 	  glEnable(GL_TEXTURE_2D); */
/* 	  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray6); */

/* 	  glPushMatrix(); */
/* 	  glTranslatef(-(-(BOARDSIZE-3.5)+(BOARDSIZE-1)/2.0), 0.0,  */
/* 		       -(2.4+BOARDSIZE+(BOARDSIZE-1)/2.0)); */
/* 	  draw_board(); */
/* 	  glPopMatrix(); */
/* 	  glDisable(GL_TEXTURE_2D); */
/* 	} */
/* 	/\* reset *\/ */
/* 	if(elevator > 2.0) { */
/* 	  antposition[i][0] = -4.0;/\*-= -(-(BOARDSIZE-3.5)+(BOARDSIZE-1)/2.0);*\//\*= -4.0;*\/ */
/* 	  antposition[i][1] = 5.5;/\*-(2.4+BOARDSIZE+(BOARDSIZE-1)/2.0);*\/ */
/* /\* 	  antposition[i][2] = 0.15; *\/ */
/* 	  antdirection[i] = PI/2.0; */
/* 	  bposition[i][0] = 0; */
/* 	  bposition[i][1] = 8; */
/* 	  part[i] = 0; */
/* 	  antvelocity[i] = 0.02; */
/* 	  fir = 0; */
/* 	  antmaterial[i] = MaterialRed; */

/* 	  makenew = 1; */

/* 	  elevator = 0.0; */
/* 	  introduced = 200; */
/* 	} */
/* 	else { */
/* 	  part[i]--; */
/* 	} */
/*       } */
    
    }
    
    /* move toward goal, correct ant direction if required */
    else {
      
      /* difference */
      double dx = antpath[i][part[i]][0] - antposition[i][0];
      double dz = - antpath[i][part[i]][1] + antposition[i][1];
      double theta, ideal;

      if(dz > EPSILON)
	theta = atan(dz/dx);
      else
	theta = dx > EPSILON ? 0.0 : PI;
      
      ideal = theta - antdirection[i];
      if(ideal < -Pi/2.0)
	ideal += Pi;

      /* compute correction */
      {
        double dt = sign(ideal) * min(fabs(ideal), PI/90.0);
        antdirection[i] += dt;
        if(antdirection[i] > 2.0*PI)
          antdirection[i] = 0.0;
      }
    }
    
    antposition[i][0] += antvelocity[i] * cos(antdirection[i]);
    antposition[i][1] += antvelocity[i] * sin(-antdirection[i]);
  }
}

static void pinit(void) {
  glClearDepth(1.0);
  glClearColor(0.0, 0.0, 0.0, 1.0);
  
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, position0);
  glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT1, GL_POSITION, position1);

  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.05);
  glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.001);
  glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.1);

  glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.05);
  glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.001);
  glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.1);


/*   glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient); */
/*   glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside); */
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  glEnable(GL_NORMALIZE);
  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);
  
  /* moebius */
  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);

  glShadeModel(GL_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);

  /* setup textures */
  makeCheckImage();
  makeBrushedImage();

  build_board(0);
  build_board(1);

/*   makeCheckImage(); */
/*   glPixelStorei(GL_UNPACK_ALIGNMENT, 1); */
/*   glTexImage2D(GL_TEXTURE_2D, 0, 3, checkImageWidth,  */
/* 	       checkImageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, checkers); */
/*   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); */
/*   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); */

/*   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); */
/*   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); */
/*   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); */
  glEnable(GL_TEXTURE_2D);
    
/*   glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess); */
/*   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular); */
}

void release_moebius(ModeInfo * mi) {
  if(moebius) {
	free((void *) moebius);
	moebius = (moebiusstruct *) NULL;
  }
  FreeAllGL(mi);
}

int mag = 4.0;
#define MAX_MAGNIFICATION 10
#define max(a, b) a < b ? b : a
#define min(a, b) a < b ? a : b

Bool moebius_handle_event (ModeInfo *mi, XEvent *event) {
  moebiusstruct *mp = &moebius[MI_SCREEN(mi)];

  switch(event->xany.type) {
  case ButtonPress:

    switch(event->xbutton.button) {

    case Button1:
      mp->button_down_p = True;
      gltrackball_start(mp->trackball, 
			event->xbutton.x, event->xbutton.y,
			MI_WIDTH (mi), MI_HEIGHT (mi));
      break;

    case Button3:
      focus = (focus + 1) % ANTCOUNT;
      break;
      
    case Button4:
      mag = max(mag-1, 1);
      break;

    case Button5:
      mag = min(mag+1, MAX_MAGNIFICATION);
      break;
    }

    break;
    
  case ButtonRelease:

    switch(event->xbutton.button) {
    case Button1:
      mp->button_down_p = False;
      break;
    }

    break;

  case MotionNotify:
    if(mp->button_down_p)
      gltrackball_track(mp->trackball,
			event->xmotion.x, event->xmotion.y,
			MI_WIDTH (mi), MI_HEIGHT (mi));
    break;
    
  default:
    return False;
  }

  return True;
}

void init_moebius(ModeInfo * mi) {
  double rot_speed = 0.3;

  moebiusstruct *mp;
  
  if (moebius == NULL) {
	if ((moebius = (moebiusstruct *) calloc(MI_NUM_SCREENS(mi),
						sizeof (moebiusstruct))) == NULL)
	  return;
  }
  mp = &moebius[MI_SCREEN(mi)];
  mp->step = NRAND(90);
  mp->ant_position = NRAND(90);

  mp->rot = make_rotator (rot_speed, rot_speed, rot_speed, 1, 0, True);
  mp->trackball = gltrackball_init ();
  
  if ((mp->glx_context = init_GL(mi)) != NULL) {
    reshape_moebius(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    glDrawBuffer(GL_BACK);
    pinit();
  } 
  else
    MI_CLEARWINDOW(mi);
}

void draw_moebius(ModeInfo * mi) {
  double h = (GLfloat) MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);

  moebiusstruct *mp;
  
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  
  if(!moebius)
	return;
  mp = &moebius[MI_SCREEN(mi)];
  
  MI_IS_DRAWN(mi) = True;
  
  if(!mp->glx_context)
	return;
  
  glXMakeCurrent(display, window, *(mp->glx_context));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* first panel */
  glPushMatrix();
/*   h = ((GLfloat) MI_HEIGHT(mi)/2) / (3*(GLfloat)MI_WIDTH(mi)/4); */
  glViewport(MI_WIDTH(mi)/32, MI_HEIGHT(mi)/8, (9*MI_WIDTH(mi))/16, 3*MI_HEIGHT(mi)/4);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

/*   h = (3*MI_HEIGHT(mi)/4) / (3*MI_WIDTH(mi)/4); */
  gluPerspective(45, 1/h, 0.001, 25.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glPushMatrix();

  /* follow focused ant */
  glTranslatef(0.0, 0.0, -mag - 5.0);
  glRotatef(20.0+5.0*sin(ant_step/40.0), 1.0, 0.0, 0.0);
/*   glTranslatef(0.0,  */
/* 	       started ? -mag : -8.0 + 4.0*fabs(sin(ant_step/10.0)),  */
/* 	       started ? -mag : -8.0 + 4.0*fabs(sin(ant_step/10.0))); */

  gltrackball_rotate(mp->trackball);

  glRotatef(ant_step*0.6, 0.0, 1.0, 0.0);

/*   glRotatef(90.0, 0.0, 0.0, 1.0); */

/*   glTranslatef(-antposition[0][0]-0.5, 0.0, -antposition[focus][1]); */
  /*-elevator*/

  /* sync */
  if(!draw_moebius_strip(mi)) {
    release_moebius(mi);
    return;
  }

  glPopMatrix();
  glPopMatrix();

  h = (GLfloat) (3*MI_HEIGHT(mi)/8) / (GLfloat) (MI_WIDTH(mi)/2);

  /* draw overhead */
  glPushMatrix();
  glViewport((17*MI_WIDTH(mi))/32, MI_HEIGHT(mi)/2, MI_WIDTH(mi)/2, 3*MI_HEIGHT(mi)/8);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1/h, 0.001, 25.0);
  glMatrixMode(GL_MODELVIEW);
    
  /* twist scene */
  glTranslatef(0.0, 0.0, -16.0);
  glRotatef(60.0, 1.0, 0.0, 0.0);
  glRotatef(-15.0 + ant_step/10.0, 0.0, 1.0, 0.0);
  gltrackball_rotate(mp->trackball);

  /* sync */
  if(!draw_moebius_strip(mi)) {
    release_moebius(mi);
    return;
  }

  glPopMatrix();

  /* draw ant display */
  glPushMatrix();
  glViewport((5*MI_WIDTH(mi))/8, MI_HEIGHT(mi)/8, (11*MI_WIDTH(mi))/32, 3*MI_HEIGHT(mi)/8);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1/h, 0.001, 25.0);
  glMatrixMode(GL_MODELVIEW);
    
  /* twist scene */
  glTranslatef(0.0, 0.0, -1.6);
  glRotatef(30.0, 1.0, 0.0, 0.0);
  glRotatef(ant_step, 0.0, 1.0, 0.0);
  glRotatef(90.0, 0.0, 0.0, 1.0);
  
/*   /\* draw ant shadow *\/ */
/*   glPushMatrix(); */
/*   glScalef(1.0, 0.01, 1.0); */
/*   glRotatef(90.0, 0.0, 0.0, 1.0); */
/*   glRotatef(90.0, 0.0, 1.0, 0.0); */
/*   glDisable(GL_LIGHTING); */
/*   glColor4fv(MaterialGray6); */
  
/*   /\* slow down first ant *\/ */
/*   draw_ant(MaterialGrayB, 0, 1, first_ant_step, mySphere, myCone); */
/*   glPopMatrix(); */

  /* draw ant body */
  glEnable(GL_TEXTURE_2D);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
  glBindTexture(GL_TEXTURE_2D, brushedtexture);
  draw_ant(MaterialGray35, 0, 1, ant_step/2.0, mySphereTex, myCone2);
  glDisable(GL_TEXTURE_2D);

  glPopMatrix();

/*   /\* draw overlay *\/ */
/*   glPushMatrix(); */

/*   /\* go to ortho mode *\/ */
/*   glViewport(MI_WIDTH(mi)/2, MI_HEIGHT(mi)/8, MI_WIDTH(mi)/2, 3*MI_HEIGHT(mi)/8); */

/*   glMatrixMode(GL_PROJECTION); */
/*   glLoadIdentity(); */

/*   glPushMatrix (); */
/*   glOrtho(-4.0, 4.0, -3.0, 3.0, -100.0, 100.0); */

/*   glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGrayB); */
/*   glColor4fv(MaterialGrayB); */

/*   glDisable(GL_LIGHTING); */
/*   glEnable(GL_BLEND); */

/*   glBegin(GL_QUADS); */
/*   glNormal3f(0.0, 0.0, 1.0); */
/*   glVertex3f(4.0, 3.0, 0.0); */
/*   glVertex3f(2.0, 3.0, 0.0); */
/*   glVertex3f(2.0, -3.0, 0.0); */
/*   glVertex3f(4.0, -3.0, 0.0); */
/*   glEnd(); */

/*   glEnable(GL_LIGHTING); */
/*   glDisable(GL_BLEND); */

/*   glPopMatrix(); */
/*   glPopMatrix(); */
  
  if (MI_IS_FPS(mi)) do_fps (mi);
  glFlush();
  
  glXSwapBuffers(display, window);
  
  update_ants();

  mp->step += 0.025;
}

void change_moebius(ModeInfo * mi) {
  moebiusstruct *mp = &moebius[MI_SCREEN(mi)];
  
  if (!mp->glx_context)
	return;
  
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(mp->glx_context));
  pinit();
}

#endif
