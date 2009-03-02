/* blinkbox, Copyright (c) 2003 Jeremy English <jenglish@myself.com>
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

extern XtAppContext app;

#define PROGCLASS	  "BlinkBox"
#define HACK_INIT	  init_ball
#define HACK_DRAW	  draw_ball
#define HACK_RESHAPE  reshape_ball
#define sws_opts	  xlockmore_opts

#define MAX_COUNT 20

#define DEFAULTS	"*delay:	30000       \n" \

#include "xlockmore.h"
#include "sphere.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */


#include <GL/glu.h>

typedef struct{
  GLfloat x,y,z;
} Tdpos;

typedef struct{
  int hit;
  Tdpos pos;
  int counter;
  GLfloat color[3];
  GLfloat rot[4];
}Side;

struct Bounding_box {
  Tdpos top;
  Tdpos bottom;
} bbox = {{7,7,10},{-7,-7,-10}};

struct Ball {
  GLfloat x;
  GLfloat y;
  GLfloat z;
  int d;
} ball = {0,0,0,1};

static GLuint ballList;
static GLuint boxList;
Tdpos mo = { 1.0, 1.0, 1.0};  /*motion*/
Tdpos moh= {-1.0,-1.5,-1.5}; /*hold motion value*/

GLfloat bscale[3] = {1,1,0.25};
GLfloat bpos[3];

/*sides*/
static Side lside = {0, {0, 0, 0,}, MAX_COUNT,  {1.0, 0.0, 0.0},{90,0,1,0}};/*Red*/
static Side rside = {0, {0, 0, 0,}, MAX_COUNT,  {0.0, 1.0, 0.0},{90,0,1,0}};/*Green*/
static Side tside = {0, {0, 0, 0,}, MAX_COUNT,  {0.0, 0.0, 1.0},{90,1,0,0}};/*Blue*/
static Side bside = {0, {0, 0, 0,}, MAX_COUNT,  {1.0, 0.5, 0.0},{90,1,0,0}};/*Orange*/
static Side fside = {0, {0, 0, 0,}, MAX_COUNT,  {1.0, 1.0, 0.0},{90,0,0,1}};/*Yellow*/
static Side aside = {0, {0, 0, 0,}, MAX_COUNT,  {0.5, 0.0, 1.0},{90,0,0,1}};/*Purple*/
Side *sp;

/* lights */
static float LightDiffuse[]=   { 1.0f, 1.0f, 1.0f, 1.0f };
static float LightPosition[]=  { 20.0f, 100.0f, 20.0f, 1.0f };

ModeSpecOpt sws_opts = {0,NULL,0,NULL,NULL};

void
swap(GLfloat *a, GLfloat *b)
{
  GLfloat t = *a;
  *a = *b;
  *b = t;
}

float 
get_rand(void)
{
  GLfloat j = 1+(random() % 2);
  return (j);
}

void
swap_mov(GLfloat *a, GLfloat *b)
{
  int j;
  swap(a,b);
  j = get_rand();
  if (*a < 0)
    *a = (j *= -1);
  else
    *a = j;
}

void 
cp_b_pos(Tdpos *s_pos){
  s_pos->x = ball.x;
  s_pos->y = ball.y;
  s_pos->z = ball.z;
}

void
hit_side(void)
{
  if ((ball.x - ball.d) <= bbox.bottom.x){ 
    lside.hit = 1;
    lside.counter = MAX_COUNT;
    cp_b_pos(&lside.pos);
    swap_mov(&mo.x,&moh.x);
  }else
  if ((ball.x + ball.d) >= bbox.top.x){ 
    rside.hit = 1;
    rside.counter = MAX_COUNT;
    cp_b_pos(&rside.pos);
    swap_mov(&mo.x,&moh.x);
  }
}

void
hit_top_bottom(void)
{
  if ((ball.y - ball.d) <= bbox.bottom.y){
    bside.hit = 1;
    bside.counter = MAX_COUNT;    
    cp_b_pos(&bside.pos);
    swap_mov(&mo.y,&moh.y);
  }else
  if ((ball.y + ball.d) >= bbox.top.y){
    tside.hit = 1;
    tside.counter = MAX_COUNT;
    cp_b_pos(&tside.pos);
    swap_mov(&mo.y,&moh.y);
  }
}

void
hit_front_back(void)
{    
  if ((ball.z - ball.d) <= bbox.bottom.z){
    aside.hit = 1;
    aside.counter = MAX_COUNT;
    cp_b_pos(&aside.pos);
    swap_mov(&mo.z,&moh.z);
  }else
  if((ball.z + ball.d) >= bbox.top.z){
    fside.hit = 1;
    fside.counter = MAX_COUNT;
    cp_b_pos(&fside.pos);
    swap_mov(&mo.z,&moh.z);
  }
}

void
reshape_ball (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glViewport (0, 0, (GLint) width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 40.0,
             0.0, 0.0, 0.0,
             0.0, 2.0,  10.0);

}

void
unit_cube(void)
{
  glBegin(GL_QUADS);		
  glNormal3f( 0.0f, -1.0f, 0.0f);
  glVertex3f(-1.0f, -1.0f, -1.0f);	
  glVertex3f( 1.0f, -1.0f, -1.0f);
  glVertex3f( 1.0f, -1.0f,  1.0f);
  glVertex3f(-1.0f, -1.0f,  1.0f);
  glNormal3f( 0.0f,  0.0f,  1.0f);
  glVertex3f(-1.0f, -1.0f,  1.0f);
  glVertex3f( 1.0f, -1.0f,  1.0f);
  glVertex3f( 1.0f,  1.0f,  1.0f);
  glVertex3f(-1.0f,  1.0f,  1.0f);
  glNormal3f( 0.0f,  0.0f, -1.0f);
  glVertex3f(-1.0f, -1.0f, -1.0f);
  glVertex3f(-1.0f,  1.0f, -1.0f);
  glVertex3f( 1.0f,  1.0f, -1.0f);
  glVertex3f( 1.0f, -1.0f, -1.0f);	
  glNormal3f( 1.0f,  0.0f,  0.0f);
  glVertex3f( 1.0f, -1.0f, -1.0f);
  glVertex3f( 1.0f,  1.0f, -1.0f);
  glVertex3f( 1.0f,  1.0f,  1.0f);
  glVertex3f( 1.0f, -1.0f,  1.0f);
  glNormal3f( -1.0f, 0.0f,  0.0f);
  glVertex3f(-1.0f, -1.0f, -1.0f);
  glVertex3f(-1.0f, -1.0f,  1.0f);
  glVertex3f(-1.0f,  1.0f,  1.0f);
  glVertex3f(-1.0f,  1.0f, -1.0f);
  glNormal3f( 1.0f,  1.0f,  0.0f);
  glVertex3f(-1.0f,  1.0f, -1.0f);
  glVertex3f(-1.0f,  1.0f,  1.0f);
  glVertex3f( 1.0f,  1.0f,  1.0f);
  glVertex3f( 1.0f,  1.0f, -1.0f);
  glEnd();						
}

void 
init_ball (ModeInfo *mi)
{ 
  #define SPHERE_SLICES 32  /* how densely to render spheres */
  #define SPHERE_STACKS 16
  sp = malloc(sizeof(*sp));
  if(sp == NULL){
    fprintf(stderr,"Could not allocate memory\n");
    exit(1);
  }
  init_GL(mi);
  reshape_ball(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  ballList = glGenLists(1);
  glNewList(ballList, GL_COMPILE);
  unit_sphere (SPHERE_STACKS, SPHERE_SLICES, False);
  glEndList ();

  boxList = glGenLists(1);
  glNewList(boxList, GL_COMPILE);
  unit_cube();
  glEndList();

  glEnable(GL_COLOR_MATERIAL);
  glShadeModel(GL_SMOOTH);				
  glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
  glClearDepth(1.0f);				
  glEnable(GL_DEPTH_TEST);		
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_LIGHTING);
  glClearDepth(1);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);
  glLightfv(GL_LIGHT1, GL_POSITION,LightPosition);
  glEnable(GL_LIGHT1);
}

void
draw_ball (ModeInfo *mi)
{  
   Display *dpy = MI_DISPLAY(mi);
   Window window = MI_WINDOW(mi);
   /*int dem = 1;*/
   int i = 0;
   glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   hit_top_bottom();
   hit_front_back();
   hit_side();

   glRotated(0.25,0,0,1);
   glRotated(0.25,0,1,0);
   glRotated(0.25,1,0,0);

   glColor3f(1,1,1);
   glPushMatrix();
   glTranslatef(ball.x += mo.x,
                ball.y += mo.y,
                ball.z += mo.z);

   glCallList(ballList);
   glPopMatrix();

   while(i < 6){
    switch(i){
      case 0:{
               sp = &lside;
               bpos[0] = lside.pos.z*-1;
               bpos[1] = lside.pos.y;
               bpos[2] = bbox.bottom.x - bscale[3];
               break;
             }
      case 1:{
               sp = &rside;
               bpos[0] = rside.pos.z*-1;
               bpos[1] = rside.pos.y;
               bpos[2] = bbox.top.x + bscale[3];
               break;
             }
      case 2:{
               sp = &tside;
               bpos[0] = tside.pos.x;
               bpos[1] = tside.pos.z;
               bpos[2] = bbox.bottom.y - bscale[3];
               break;
             }
      case 3:{
               sp = &bside;
               bpos[0] = bside.pos.x;
               bpos[1] = bside.pos.z;
               bpos[2] = bbox.top.y + bscale[3];
               break;
             }
      case 4:{
               sp = &fside;
               bpos[0] = fside.pos.y;
               bpos[1] = fside.pos.x*-1;
               bpos[2] = bbox.top.z + bscale[3];
               break;
             }
      case 5:{
               sp = &aside;
               bpos[0] = aside.pos.y;
               bpos[1] = aside.pos.x*-1;
               bpos[2] = bbox.bottom.z + bscale[3];
               break;
             }
    }
    if(sp->hit){
     glColor3fv(sp->color);
     glPushMatrix();
     glRotatef(sp->rot[0],sp->rot[1],sp->rot[2],sp->rot[3]);
     glTranslatef(bpos[0],bpos[1],bpos[2]);
     /*dem = (MAX_COUNT-(sp->counter+1));*/
     /*glScalef(bscale[0]/dem,bscale[1]/dem,bscale[2]);*/
     glScalef(bscale[0],bscale[1],bscale[2]);
     glCallList(boxList);
     glPopMatrix();
     sp->counter--;
     if(!sp->counter)
       sp->hit = 0;
    }
    i++;
  }


   glFinish();
   glXSwapBuffers(dpy, window);

}

#endif /* USE_GL */
