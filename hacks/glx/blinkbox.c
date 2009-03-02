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
#define ALPHA_AMT 0.05

/* this should be between 1 and 8 */
#define DEF_WH       "2"
#define DEF_DISSOLVE "False"
#define DEF_FADE     "True"

#define DEFAULTS	"*delay:	30000		 \n" \
			"*wireframe:	False		 \n" \
			"*boxsize:  "	DEF_WH		"\n" \
			"*dissolve: "	DEF_DISSOLVE	"\n" \
			"*fade:	"	DEF_FADE	"\n" \

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

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
  int des_count;
  int alpha_count;
}Side;

struct Bounding_box {
  Tdpos top;
  Tdpos bottom;
} bbox = {{14,14,20},{-14,-14,-20}};

struct Ball {
  GLfloat x;
  GLfloat y;
  GLfloat z;
  int d;
} ball = {0,0,0,1};

struct {
  GLfloat wh; /*width Height*/
  GLfloat d; /*depth*/
} bscale = {1, 0.25};

static GLuint ballList;
static GLuint boxList;
Tdpos mo = { 1.0, 1.0, 1.0};  /*motion*/
Tdpos moh= {-1.0,-1.5,-1.5}; /*hold motion value*/

Tdpos bpos= {1.0, 1.0, 1.0};

/*sides*/
static Side lside = {0, {0, 0, 0,}, MAX_COUNT,  {1.0, 0.0, 0.0},{90,0,1,0},1,1};/*Red*/
static Side rside = {0, {0, 0, 0,}, MAX_COUNT,  {0.0, 1.0, 0.0},{90,0,1,0},1,1};/*Green*/
static Side tside = {0, {0, 0, 0,}, MAX_COUNT,  {0.0, 0.0, 1.0},{90,1,0,0},1,1};/*Blue*/
static Side bside = {0, {0, 0, 0,}, MAX_COUNT,  {1.0, 0.5, 0.0},{90,1,0,0},1,1};/*Orange*/
static Side fside = {0, {0, 0, 0,}, MAX_COUNT,  {1.0, 1.0, 0.0},{90,0,0,1},1,1};/*Yellow*/
static Side aside = {0, {0, 0, 0,}, MAX_COUNT,  {0.5, 0.0, 1.0},{90,0,0,1},1,1};/*Purple*/
Side *sp;

/* lights */
static float LightDiffuse[]=   { 1.0f, 1.0f, 1.0f, 1.0f };
static float LightPosition[]=  { 20.0f, 100.0f, 20.0f, 1.0f };

static Bool do_dissolve;
static Bool do_fade;
static GLfloat des_amt   = 1;

static XrmOptionDescRec opts[] = {
  { "-boxsize",  ".boxsize",  XrmoptionSepArg, 0      },
  { "-dissolve", ".dissolve", XrmoptionNoArg, "True"  },
  { "+dissolve", ".dissolve", XrmoptionNoArg, "False" },
  { "-fade",     ".fade",     XrmoptionNoArg, "True"  },
  { "+fade",     ".fade",     XrmoptionNoArg, "False" }

};

static argtype vars[] = {
  {&bscale.wh,   "boxsize",   "Boxsize",  DEF_WH,       t_Float},
  {&do_dissolve, "dissolve",  "Dissolve", DEF_DISSOLVE, t_Bool},
  {&do_fade,     "fade",      "Fade",     DEF_FADE,     t_Bool},
};

ModeSpecOpt sws_opts = {countof(opts), opts, countof(vars), vars, NULL};

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
    lside.counter   = MAX_COUNT;
    lside.des_count = 1;
    lside.alpha_count = 0;
    cp_b_pos(&lside.pos);
    swap_mov(&mo.x,&moh.x);
  }else
  if ((ball.x + ball.d) >= bbox.top.x){
    rside.hit = 1;
    rside.counter = MAX_COUNT;
    rside.des_count = 1;
    rside.alpha_count = 0;
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
    bside.des_count = 1;
    bside.alpha_count = 0;
    cp_b_pos(&bside.pos);
    swap_mov(&mo.y,&moh.y);
  }else
  if ((ball.y + ball.d) >= bbox.top.y){
    tside.hit = 1;
    tside.counter = MAX_COUNT;
    tside.des_count = 1;
    tside.alpha_count = 0;
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
    aside.des_count = 1;
    aside.alpha_count = 0;
    cp_b_pos(&aside.pos);
    swap_mov(&mo.z,&moh.z);
  }else
  if((ball.z + ball.d) >= bbox.top.z){
    fside.hit = 1;
    fside.counter = MAX_COUNT;
    fside.des_count = 1;
    fside.alpha_count = 0;
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
unit_cube(int wire)
{
  glBegin((wire)?GL_LINE_LOOP:GL_QUADS);
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
  int wire = MI_IS_WIREFRAME(mi);

  sp = malloc(sizeof(*sp));
  if(sp == NULL){
    fprintf(stderr,"Could not allocate memory\n");
    exit(1);
  }
  if( (bscale.wh < 1) ||
      (bscale.wh > 8) ) {
    fprintf(stderr,"Boxsize out of range. Using default\n");
    bscale.wh = 2;
  }
  if (do_dissolve){
    des_amt = bscale.wh / MAX_COUNT;
  }
  init_GL(mi);
  reshape_ball(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  ballList = glGenLists(1);
  glNewList(ballList, GL_COMPILE);
  unit_sphere (SPHERE_STACKS, SPHERE_SLICES, wire);
  glEndList ();

  boxList = glGenLists(1);
  glNewList(boxList, GL_COMPILE);
  unit_cube(wire);
  glEndList();

  if (wire) return;

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
  if (do_fade){
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
  }
}

void
CheckBoxPos(GLfloat bot_x, GLfloat top_x, GLfloat bot_y, GLfloat top_y)
{
  /*Make sure it's inside of the bounding box*/
  bpos.x = ((bpos.x - bscale.wh) < bot_x) ? bot_x + bscale.wh : bpos.x;
  bpos.x = ((bpos.x + bscale.wh) > top_x) ? top_x - bscale.wh : bpos.x;
  bpos.y = ((bpos.y - bscale.wh) < bot_y) ? bot_y + bscale.wh : bpos.y;
  bpos.y = ((bpos.y + bscale.wh) > top_y) ? top_y - bscale.wh : bpos.y;
}

void
draw_ball (ModeInfo *mi)
{
   Display *dpy = MI_DISPLAY(mi);
   Window window = MI_WINDOW(mi);
   int i = 0;
   glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   hit_top_bottom();
   hit_front_back();
   hit_side();

   glRotated(0.25,0,0,1);
   glRotated(0.25,0,1,0);
   glRotated(0.25,1,0,0);


   glPushMatrix();
   glScalef(0.5,0.5,0.5);

   glColor3f(1,1,1);
   glPushMatrix();
   glTranslatef(ball.x += mo.x,
                ball.y += mo.y,
                ball.z += mo.z);

   glScalef(2,2,2);
   glCallList(ballList);
   glPopMatrix();

   while(i < 6){
    switch(i){
      case 0:{
               sp = &lside;
               bpos.x = lside.pos.z*-1;
               bpos.y = lside.pos.y;
               bpos.z = bbox.bottom.x - bscale.d;
               if (sp->hit)
                CheckBoxPos(bbox.bottom.z,bbox.top.z,bbox.bottom.y,bbox.top.y);
               break;
             }
      case 1:{
               sp = &rside;
               bpos.x = rside.pos.z*-1;
               bpos.y = rside.pos.y;
               bpos.z = bbox.top.x + bscale.d;
               if (sp->hit)
                CheckBoxPos(bbox.bottom.z,bbox.top.z,bbox.bottom.y,bbox.top.y);
               break;
             }
      case 2:{
               sp = &tside;
               bpos.x = tside.pos.x;
               bpos.y = tside.pos.z;
               bpos.z = bbox.bottom.y - bscale.d;
               if (sp->hit)
                CheckBoxPos(bbox.bottom.x,bbox.top.x,bbox.bottom.z,bbox.top.z);
               break;
             }
      case 3:{
               sp = &bside;
               bpos.x = bside.pos.x;
               bpos.y = bside.pos.z;
               bpos.z = bbox.top.y + bscale.d;
               if (sp->hit)
                CheckBoxPos(bbox.bottom.x,bbox.top.x,bbox.bottom.z,bbox.top.z);
               break;
             }
      case 4:{
               sp = &fside;
               bpos.x = fside.pos.y;
               bpos.y = fside.pos.x*-1;
               bpos.z = bbox.top.z + bscale.d;
               if (sp->hit)
                CheckBoxPos(bbox.bottom.y,bbox.top.y,bbox.bottom.x,bbox.top.x);
               break;
             }
      case 5:{
               sp = &aside;
               bpos.x = aside.pos.y;
               bpos.y = aside.pos.x*-1;
               bpos.z = bbox.bottom.z + bscale.d;
               if (sp->hit)
                CheckBoxPos(bbox.bottom.y,bbox.top.y,bbox.bottom.x,bbox.top.x);
               break;
             }
    }
    if(sp->hit){
      if(do_fade){
        glColor4f(sp->color[0],sp->color[1],sp->color[2],1-(ALPHA_AMT * sp->alpha_count));
      }else{
        glColor3fv(sp->color);
      }
      glBlendFunc(GL_SRC_ALPHA,GL_ONE);
      glPushMatrix();
      glRotatef(sp->rot[0],sp->rot[1],sp->rot[2],sp->rot[3]);
      glTranslatef(bpos.x,bpos.y,bpos.z);
      if (do_dissolve) {
         glScalef(bscale.wh-(des_amt*sp->des_count),bscale.wh-(des_amt*sp->des_count),bscale.d);
      }else{
        glScalef(bscale.wh,bscale.wh,bscale.d);
      }
      glCallList(boxList);
      glPopMatrix();
      sp->counter--;
      sp->des_count++;
      sp->alpha_count++;
      if(!sp->counter)
      {
        sp->hit = 0;
      }
    }
    i++;
  }


   glPopMatrix();
   glFinish();
   glXSwapBuffers(dpy, window);

}

#endif /* USE_GL */
