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

/* motion blur added March 2005 by John Boero <jlboero@cs.uwm.edu>
 */

#define DEFAULTS	"*delay:	30000		 \n" \
			"*wireframe:	False		 \n" \

# define refresh_ball 0
# define release_ball 0
# define ball_handle_event 0
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "sphere.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define MAX_COUNT 20
#define ALPHA_AMT 0.05

/* this should be between 1 and 8 */
#define DEF_BOXSIZE  "2"
#define DEF_DISSOLVE "False"
#define DEF_FADE     "True"
#define DEF_BLUR     "True"


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
};

struct Ball {
  GLfloat x;
  GLfloat y;
  GLfloat z;
  int d;
};

struct bscale {
  GLfloat wh; /*width Height*/
  GLfloat d; /*depth*/
};

static const struct Bounding_box bbox = {{14,14,20},{-14,-14,-20}};

typedef struct {
  GLXContext *glx_context;

  struct Ball ball;

  struct bscale bscale;

  Tdpos mo;  /*motion*/
  Tdpos moh; /*hold motion value*/

  Tdpos bpos;

  GLuint ballList;
  GLuint boxList;
  GLfloat des_amt;

  /*sides*/
  Side lside;/*Red*/
  Side rside;/*Green*/
  Side tside;/*Blue*/
  Side bside;/*Orange*/
  Side fside;/*Yellow*/
  Side aside;/*Purple*/
  Side *sp;

} blinkboxstruct;

static blinkboxstruct *blinkbox = (blinkboxstruct *) NULL;


/* lights */
static const float LightDiffuse[]=   { 1.0f, 1.0f, 1.0f, 1.0f };
static const float LightPosition[]=  { 20.0f, 100.0f, 20.0f, 1.0f };

static Bool do_dissolve;
static Bool do_fade;
static Bool do_blur;
static float bscale_wh;

static XrmOptionDescRec opts[] = {
  { "-boxsize",  ".boxsize",  XrmoptionSepArg, 0      },
  { "-dissolve", ".dissolve", XrmoptionNoArg, "True"  },
  { "+dissolve", ".dissolve", XrmoptionNoArg, "False" },
  { "-fade",     ".fade",     XrmoptionNoArg, "True"  },
  { "+fade",     ".fade",     XrmoptionNoArg, "False" },
  { "-blur",     ".blur",     XrmoptionNoArg, "True"  },
  { "+blur",     ".blur",     XrmoptionNoArg, "False" }

};

static argtype vars[] = {
  {&bscale_wh,   "boxsize",   "Boxsize",  DEF_BOXSIZE,  t_Float},
  {&do_dissolve, "dissolve",  "Dissolve", DEF_DISSOLVE, t_Bool},
  {&do_fade,     "fade",      "Fade",     DEF_FADE,     t_Bool},
  {&do_blur,     "blur",      "Blur",     DEF_BLUR,     t_Bool},
};

ENTRYPOINT ModeSpecOpt ball_opts = {countof(opts), opts, countof(vars), vars, NULL};

static void
swap(GLfloat *a, GLfloat *b)
{
  GLfloat t = *a;
  *a = *b;
  *b = t;
}

static float
get_rand(void)
{
  GLfloat j = 1+(random() % 2);
  return (j);
}

static void
swap_mov(GLfloat *a, GLfloat *b)
{
  int j;
  swap(a,b);
  j = get_rand();
  if (*a < 0)
    *a = j * -1;
  else
    *a = j;
}

static void
cp_b_pos(blinkboxstruct *bp, Tdpos *s_pos)
{
  s_pos->x = bp->ball.x;
  s_pos->y = bp->ball.y;
  s_pos->z = bp->ball.z;
}

static void
hit_side(blinkboxstruct *bp)
{
  if ((bp->ball.x - bp->ball.d) <= bbox.bottom.x){
    bp->lside.hit = 1;
    bp->lside.counter   = MAX_COUNT;
    bp->lside.des_count = 1;
    bp->lside.alpha_count = 0;
    cp_b_pos(bp, &bp->lside.pos);
    swap_mov(&bp->mo.x,&bp->moh.x);
  }else
  if ((bp->ball.x + bp->ball.d) >= bbox.top.x){
    bp->rside.hit = 1;
    bp->rside.counter = MAX_COUNT;
    bp->rside.des_count = 1;
    bp->rside.alpha_count = 0;
    cp_b_pos(bp, &bp->rside.pos);
    swap_mov(&bp->mo.x,&bp->moh.x);
  }
}

static void
hit_top_bottom(blinkboxstruct *bp)
{
  if ((bp->ball.y - bp->ball.d) <= bbox.bottom.y){
    bp->bside.hit = 1;
    bp->bside.counter = MAX_COUNT;
    bp->bside.des_count = 1;
    bp->bside.alpha_count = 0;
    cp_b_pos(bp, &bp->bside.pos);
    swap_mov(&bp->mo.y,&bp->moh.y);
  }else
  if ((bp->ball.y + bp->ball.d) >= bbox.top.y){
    bp->tside.hit = 1;
    bp->tside.counter = MAX_COUNT;
    bp->tside.des_count = 1;
    bp->tside.alpha_count = 0;
    cp_b_pos(bp, &bp->tside.pos);
    swap_mov(&bp->mo.y,&bp->moh.y);
  }
}

static void
hit_front_back(blinkboxstruct *bp)
{
  if ((bp->ball.z - bp->ball.d) <= bbox.bottom.z){
    bp->aside.hit = 1;
    bp->aside.counter = MAX_COUNT;
    bp->aside.des_count = 1;
    bp->aside.alpha_count = 0;
    cp_b_pos(bp, &bp->aside.pos);
    swap_mov(&bp->mo.z,&bp->moh.z);
  }else
  if((bp->ball.z + bp->ball.d) >= bbox.top.z){
    bp->fside.hit = 1;
    bp->fside.counter = MAX_COUNT;
    bp->fside.des_count = 1;
    bp->fside.alpha_count = 0;
    cp_b_pos(bp, &bp->fside.pos);
    swap_mov(&bp->mo.z,&bp->moh.z);
  }
}

ENTRYPOINT void
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

static void
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

ENTRYPOINT void
init_ball (ModeInfo *mi)
{
  int wire = MI_IS_WIREFRAME(mi);
  blinkboxstruct *bp;
  
  if(blinkbox == NULL) {
    if((blinkbox = (blinkboxstruct *) calloc(MI_NUM_SCREENS(mi),
                                             sizeof (blinkboxstruct))) == NULL)
      return;
  }
  bp = &blinkbox[MI_SCREEN(mi)];

  if ((bp->glx_context = init_GL(mi)) != NULL) {
    reshape_ball(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    glDrawBuffer(GL_BACK);
  }
  else
    MI_CLEARWINDOW(mi);

  bp->ball.d = 1;
  bp->bscale.wh = bscale_wh;
  bp->bscale.d = 0.25;

  bp->mo.x = 1;
  bp->mo.y = 1;
  bp->mo.z = 1;

  bp->moh.x = -1.0;
  bp->moh.y = -1.5;
  bp->moh.z = -1.5;

  bp->bpos.x = 1;
  bp->bpos.y = 1;
  bp->bpos.z = 1;

  bp->des_amt = 1;

  bp->lside.counter = MAX_COUNT;
  bp->rside.counter = MAX_COUNT;
  bp->tside.counter = MAX_COUNT;
  bp->bside.counter = MAX_COUNT;
  bp->fside.counter = MAX_COUNT;
  bp->aside.counter = MAX_COUNT;

  bp->lside.color[0] = 1;
  bp->rside.color[1] = 1;
  bp->tside.color[2] = 1;

  bp->bside.color[0] = 1;
  bp->bside.color[1] = 0.5;

  bp->fside.color[0] = 1;
  bp->fside.color[1] = 1;

  bp->aside.color[0] = 0.5;
  bp->aside.color[2] = 1;

  bp->lside.rot[0] = 90;
  bp->rside.rot[0] = 90;
  bp->tside.rot[0] = 90;
  bp->bside.rot[0] = 90;
  bp->fside.rot[0] = 90;
  bp->aside.rot[0] = 90;

  bp->lside.rot[2] = 1;
  bp->rside.rot[2] = 1;
  bp->tside.rot[1] = 1;
  bp->bside.rot[1] = 1;
  bp->fside.rot[3] = 1;
  bp->aside.rot[3] = 1;

  bp->lside.des_count = 1;
  bp->rside.des_count = 1;
  bp->tside.des_count = 1;
  bp->bside.des_count = 1;
  bp->fside.des_count = 1;
  bp->aside.des_count = 1;

  bp->lside.alpha_count = 1;
  bp->rside.alpha_count = 1;
  bp->tside.alpha_count = 1;
  bp->bside.alpha_count = 1;
  bp->fside.alpha_count = 1;
  bp->aside.alpha_count = 1;


#define SPHERE_SLICES 12  /* how densely to render spheres */
#define SPHERE_STACKS 16

  bp->sp = malloc(sizeof(*bp->sp));
  if(bp->sp == NULL){
    fprintf(stderr,"Could not allocate memory\n");
    exit(1);
  }
  if( (bp->bscale.wh < 1) ||
      (bp->bscale.wh > 8) ) {
    fprintf(stderr,"Boxsize out of range. Using default\n");
    bp->bscale.wh = 2;
  }
  if (do_dissolve){
    bp->des_amt = bp->bscale.wh / MAX_COUNT;
  }

  reshape_ball(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  bp->ballList = glGenLists(1);
  glNewList(bp->ballList, GL_COMPILE);
  unit_sphere (SPHERE_STACKS, SPHERE_SLICES, wire);
  glEndList ();

  bp->boxList = glGenLists(1);
  glNewList(bp->boxList, GL_COMPILE);
  unit_cube(wire);
  glEndList();

  if (wire) return;

  glEnable(GL_COLOR_MATERIAL);
  glShadeModel(GL_SMOOTH);
  glClearDepth(1.0f);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_LIGHTING);
  glClearDepth(1);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);
  glLightfv(GL_LIGHT1, GL_POSITION,LightPosition);
  glEnable(GL_LIGHT1);
  if (do_fade || do_blur) {
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
  }
}

static void
CheckBoxPos(blinkboxstruct *bp, 
            GLfloat bot_x, GLfloat top_x, GLfloat bot_y, GLfloat top_y)
{
  /*Make sure it's inside of the bounding box*/
  bp->bpos.x = ((bp->bpos.x - bp->bscale.wh) < bot_x) ? bot_x + bp->bscale.wh : bp->bpos.x;
  bp->bpos.x = ((bp->bpos.x + bp->bscale.wh) > top_x) ? top_x - bp->bscale.wh : bp->bpos.x;
  bp->bpos.y = ((bp->bpos.y - bp->bscale.wh) < bot_y) ? bot_y + bp->bscale.wh : bp->bpos.y;
  bp->bpos.y = ((bp->bpos.y + bp->bscale.wh) > top_y) ? top_y - bp->bscale.wh : bp->bpos.y;
}

ENTRYPOINT void
draw_ball (ModeInfo *mi)
{
   blinkboxstruct *bp = &blinkbox[MI_SCREEN(mi)];

   Display *dpy = MI_DISPLAY(mi);
   Window window = MI_WINDOW(mi);
   int i = 0;

   if (! bp->glx_context)
     return;
   mi->polygon_count = 0;
   glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));

   glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   hit_top_bottom(bp);
   hit_front_back(bp);
   hit_side(bp);

   glRotated(0.25,0,0,1);
   glRotated(0.25,0,1,0);
   glRotated(0.25,1,0,0);


   glPushMatrix();
   glScalef(0.5,0.5,0.5);

   glColor3f(1,1,1);
   glPushMatrix();

   if (!do_blur || MI_IS_WIREFRAME(mi)) {
     glTranslatef(bp->ball.x += bp->mo.x,
                  bp->ball.y += bp->mo.y,
                  bp->ball.z += bp->mo.z);

     glScalef(2,2,2);
     glCallList(bp->ballList);
     mi->polygon_count += SPHERE_SLICES*SPHERE_STACKS;

   } else {

#    define blur_detail 24.0
     float ball_alpha = 1 / blur_detail;

     glBlendFunc(GL_SRC_ALPHA,GL_ONE);
     glTranslatef(bp->ball.x, bp->ball.y, bp->ball.z);
   
     for (i = 0; i < blur_detail; ++i) {
       glTranslatef(bp->mo.x / blur_detail,
                    bp->mo.y / blur_detail,
                    bp->mo.z / blur_detail);

       /* comment the following line for quick but boring linear blur */
       ball_alpha = sin((M_PI / blur_detail) * i) / blur_detail;
     
       glColor4f(1, 1, 1, ball_alpha);

       glScalef(2, 2, 2);
       glCallList(bp->ballList);
       mi->polygon_count += SPHERE_SLICES*SPHERE_STACKS;
       glScalef(.5, .5, .5);
     }
     i = 0;
   
     bp->ball.x += bp->mo.x;
     bp->ball.y += bp->mo.y;
     bp->ball.z += bp->mo.z;
   }
   
   glPopMatrix();

   while(i < 6){
    switch(i){
      case 0:{
               bp->sp = &bp->lside;
               bp->bpos.x = bp->lside.pos.z*-1;
               bp->bpos.y = bp->lside.pos.y;
               bp->bpos.z = bbox.bottom.x - bp->bscale.d;
               if (bp->sp->hit)
                CheckBoxPos(bp, bbox.bottom.z,bbox.top.z,bbox.bottom.y,bbox.top.y);
               break;
             }
      case 1:{
               bp->sp = &bp->rside;
               bp->bpos.x = bp->rside.pos.z*-1;
               bp->bpos.y = bp->rside.pos.y;
               bp->bpos.z = bbox.top.x + bp->bscale.d;
               if (bp->sp->hit)
                CheckBoxPos(bp, bbox.bottom.z,bbox.top.z,bbox.bottom.y,bbox.top.y);
               break;
             }
      case 2:{
               bp->sp = &bp->tside;
               bp->bpos.x = bp->tside.pos.x;
               bp->bpos.y = bp->tside.pos.z;
               bp->bpos.z = bbox.bottom.y - bp->bscale.d;
               if (bp->sp->hit)
                CheckBoxPos(bp, bbox.bottom.x,bbox.top.x,bbox.bottom.z,bbox.top.z);
               break;
             }
      case 3:{
               bp->sp = &bp->bside;
               bp->bpos.x = bp->bside.pos.x;
               bp->bpos.y = bp->bside.pos.z;
               bp->bpos.z = bbox.top.y + bp->bscale.d;
               if (bp->sp->hit)
                CheckBoxPos(bp, bbox.bottom.x,bbox.top.x,bbox.bottom.z,bbox.top.z);
               break;
             }
      case 4:{
               bp->sp = &bp->fside;
               bp->bpos.x = bp->fside.pos.y;
               bp->bpos.y = bp->fside.pos.x*-1;
               bp->bpos.z = bbox.top.z + bp->bscale.d;
               if (bp->sp->hit)
                CheckBoxPos(bp, bbox.bottom.y,bbox.top.y,bbox.bottom.x,bbox.top.x);
               break;
             }
      case 5:{
               bp->sp = &bp->aside;
               bp->bpos.x = bp->aside.pos.y;
               bp->bpos.y = bp->aside.pos.x*-1;
               bp->bpos.z = bbox.bottom.z + bp->bscale.d;
               if (bp->sp->hit)
                CheckBoxPos(bp, bbox.bottom.y,bbox.top.y,bbox.bottom.x,bbox.top.x);
               break;
             }
    }
    if(bp->sp->hit){
      if(do_fade){
        glColor4f(bp->sp->color[0],bp->sp->color[1],bp->sp->color[2],1-(ALPHA_AMT * bp->sp->alpha_count));
      }else{
        glColor3fv(bp->sp->color);
      }
      glBlendFunc(GL_SRC_ALPHA,GL_ONE);
      glPushMatrix();
      glRotatef(bp->sp->rot[0],bp->sp->rot[1],bp->sp->rot[2],bp->sp->rot[3]);
      glTranslatef(bp->bpos.x,bp->bpos.y,bp->bpos.z);
      if (do_dissolve) {
         glScalef(bp->bscale.wh-(bp->des_amt*bp->sp->des_count),bp->bscale.wh-(bp->des_amt*bp->sp->des_count),bp->bscale.d);
      }else{
        glScalef(bp->bscale.wh,bp->bscale.wh,bp->bscale.d);
      }
      glCallList(bp->boxList);
      mi->polygon_count += 6;
      glPopMatrix();
      bp->sp->counter--;
      bp->sp->des_count++;
      bp->sp->alpha_count++;
      if(!bp->sp->counter)
      {
        bp->sp->hit = 0;
      }
    }
    i++;
  }


   glPopMatrix();
  if (mi->fps_p) do_fps (mi);
   glFinish();
   glXSwapBuffers(dpy, window);

}

XSCREENSAVER_MODULE_2 ("BlinkBox", blinkbox, ball)

#endif /* USE_GL */
