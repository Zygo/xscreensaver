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
 * Copyright 2003 Blair Tennessy
 */

#ifdef STANDALONE
#define DEFAULTS	    "*delay:   20000   \n" \
			    "*showFPS: False   \n" \
                            "*useSHM:  True    \n"

# define release_antspotlight 0
#include "xlockmore.h"
#else
#include "xlock.h"
#endif

#include "sphere.h"
#include "tube.h"
#include "rotator.h"
#include "gltrackball.h"

ENTRYPOINT ModeSpecOpt antspotlight_opts = {
  0, NULL, 0, NULL, NULL
};

#ifdef USE_MODULES
ModStruct   antspotlight_description = {
  "antspotlight", "init_antspotlight", "draw_antspotlight", 
  (char *) NULL, "draw_antspotlight", "change_antspotlight",
  (char *) NULL, &antspotlight_opts, 1000, 1, 1, 1, 4, 1.0, "",
  "draws an ant scoping the screen", 0, NULL
};
#endif

#define Scale4Window               0.3
#define Scale4Iconic               0.4

#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi M_PI
#endif

#include "ants.h"
#include "grab-ximage.h"

typedef struct {
  GLXContext *glx_context;
  rotator    *rot;
  trackball_state *trackball;
  Bool        button_down_p;

  GLfloat max_tx, max_ty;
  int mono, wire, ticks;
  GLuint screentexture;

  Ant *ant;
  double boardsize;
  GLfloat spot_direction[3];
  int mag;

  Bool mipmap_p;
  Bool waiting_for_image_p;

} antspotlightstruct;

static antspotlightstruct *antspotlight = (antspotlightstruct *) NULL;

#define NUM_SCENES      2

/* draw method for ant */
static Bool draw_ant(ModeInfo *mi, antspotlightstruct *mp,
                     const GLfloat *Material, int mono, int shadow, 
	      float ant_step, Bool (*sphere)(float), Bool (*cone)(float))
{
  
  float cos1 = cos(ant_step);
  float cos2 = cos(ant_step + 2 * Pi / 3);
  float cos3 = cos(ant_step + 4 * Pi / 3);
  float sin1 = sin(ant_step);
  float sin2 = sin(ant_step + 2 * Pi / 3);
  float sin3 = sin(ant_step + 4 * Pi / 3);
  
/* Apparently this is a performance killer on many systems...
   glEnable(GL_POLYGON_SMOOTH);
 */
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mp->mono ? MaterialGray5 : Material);
  glEnable(GL_CULL_FACE);
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
  glDisable(GL_CULL_FACE);
  
  glDisable(GL_LIGHTING);
  
  /* ANTENNAS */
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   
  glBegin(GL_LINES);
  glColor3fv(mp->mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.30, 0.00);
  glColor3fv(MaterialGray);
  glVertex3f(0.40, 0.70, 0.40);
  mi->polygon_count++;
  glColor3fv(mp->mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.30, 0.00);
  glColor3fv(MaterialGray);
  glVertex3f(0.40, 0.70, -0.40);
  mi->polygon_count++;
  glEnd();

  if(!shadow) {
    glBegin(GL_POINTS);
    glColor3fv(mp->mono ? MaterialGray6 : MaterialGray5);
    glVertex3f(0.40, 0.70, 0.40);
    mi->polygon_count++;
    glVertex3f(0.40, 0.70, -0.40);
    mi->polygon_count++;
    glEnd();
  }

  /* LEFT-FRONT ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mp->mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.05, 0.18);
  glVertex3f(0.35 + 0.05 * cos1, 0.15, 0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 + 0.05 * cos1, 0.25 + 0.1 * sin1, 0.45);
  mi->polygon_count++;
  glEnd();

  /* LEFT-CENTER ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mp->mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.00, 0.18);
  glVertex3f(0.35 + 0.05 * cos2, 0.00, 0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 + 0.05 * cos2, 0.00 + 0.1 * sin2, 0.45);
  mi->polygon_count++;
  glEnd();

  /* LEFT-BACK ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mp->mono ? MaterialGray5 : Material);
  glVertex3f(0.00, -0.05, 0.18);
  glVertex3f(0.35 + 0.05 * cos3, -0.15, 0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 + 0.05 * cos3, -0.25 + 0.1 * sin3, 0.45);
  mi->polygon_count++;
  glEnd();

  /* RIGHT-FRONT ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mp->mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.05, -0.18);
  glVertex3f(0.35 - 0.05 * sin1, 0.15, -0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 - 0.05 * sin1, 0.25 + 0.1 * cos1, -0.45);
  mi->polygon_count++;
  glEnd();

  /* RIGHT-CENTER ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mp->mono ? MaterialGray5 : Material);
  glVertex3f(0.00, 0.00, -0.18);
  glVertex3f(0.35 - 0.05 * sin2, 0.00, -0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 - 0.05 * sin2, 0.00 + 0.1 * cos2, -0.45);
  mi->polygon_count++;
  glEnd();

  /* RIGHT-BACK ARM */
  glBegin(GL_LINE_STRIP);
  glColor3fv(mp->mono ? MaterialGray5 : Material);
  glVertex3f(0.00, -0.05, -0.18);
  glVertex3f(0.35 - 0.05 * sin3, -0.15, -0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 - 0.05 * sin3, -0.25 + 0.1 * cos3, -0.45);
  mi->polygon_count++;
  glEnd();

  if(!shadow) {
    glBegin(GL_POINTS);
    glColor3fv(MaterialGray5);
    glVertex3f(-0.20 + 0.05 * cos1, 0.25 + 0.1 * sin1, 0.45);
    glVertex3f(-0.20 + 0.05 * cos2, 0.00 + 0.1 * sin2, 0.45);
    glVertex3f(-0.20 + 0.05 * cos3, -0.25 + 0.1 * sin3, 0.45);
    glVertex3f(-0.20 - 0.05 * sin1, 0.25 + 0.1 * cos1, -0.45);
    glVertex3f(-0.20 - 0.05 * sin2, 0.00 + 0.1 * cos2, -0.45);
    glVertex3f(-0.20 - 0.05 * sin3, -0.25 + 0.1 * cos3, -0.45);
    mi->polygon_count += 6;
    glEnd();
  }

  glEnable(GL_LIGHTING);

  return True;
}

/* filled sphere */
static Bool mySphere(float radius)
{
#if 0
  GLUquadricObj *quadObj;
  
  if((quadObj = gluNewQuadric()) == 0)
    return False;

  gluQuadricDrawStyle(quadObj, (GLenum) GLU_FILL);
  gluSphere(quadObj, radius, 16, 16);
  gluDeleteQuadric(quadObj);
#else
    glPushMatrix();
    glScalef (radius, radius, radius);
    glRotatef (90, 1, 0, 0);
    unit_sphere (16, 16, False);
    glPopMatrix();
#endif
  return True;
}

/* silhouette sphere */
static Bool mySphere2(float radius)
{
#if 0
  GLUquadricObj *quadObj;

  if((quadObj = gluNewQuadric()) == 0)
	return False;
  gluQuadricDrawStyle(quadObj, (GLenum) GLU_LINE);
  gluSphere(quadObj, radius, 16, 8);
  gluDeleteQuadric(quadObj);
#else
  /* #### no GLU_LINE */
    glPushMatrix();
    glScalef (radius, radius, radius);
    glRotatef (90, 1, 0, 0);
    unit_sphere (16, 16, True);
    glPopMatrix();
#endif
  return True;
}

/* no cone */
static Bool myCone2(float radius) { return True; }

static void draw_board(ModeInfo *mi, antspotlightstruct *mp)
{
  int i, j;
  double cutoff = Pi/3.0;
  double center[3];
  double centertex[2];

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, mp->screentexture);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray6);

  /* draw mesh */

  /* center is roughly spotlight position */
  center[0] = mp->ant->position[0];/* + cos(ant->direction); */
  center[1] = 0.0;
  center[2] = mp->ant->position[2];/* - 0.7*sin(ant->direction);*/

  centertex[0] = (mp->boardsize/2.0+center[0]) * mp->max_tx / mp->boardsize;
  centertex[1] = mp->max_ty - ((mp->boardsize/2.0+center[2]) * mp->max_ty / mp->boardsize);

/*   glPolygonMode(GL_FRONT, GL_LINE); */
/*   glDisable(GL_TEXTURE_2D); */

  /* 
     the vertices determined here should correspond to the illuminated
     board.  ideally the code adapts vertex distribution to the
     intensity and shape of the light.
     
     i should be finding the intersection of the cone of light and
     the board-plane.
  */
  for(i = -12; i < 12; ++i) {

    double theta1, theta2;

    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f(0.0, 1.0, 0.0);

    glTexCoord2f(centertex[0], centertex[1]);
    glVertex3f(center[0], 0.01, center[2]);

    /* watch those constants */
    theta1 = mp->ant->direction + i*(cutoff/8);
    theta2 = mp->ant->direction + (i+1)*(cutoff/8);

    for(j = 1; j <= 64; ++j) {
      double point[3], tex[2];
      /* double fj = pow(1.05, j) - 1.0;*/
      double fj = j / 6.0;
      point[0] = center[0] + fj*cos(theta1);
      point[1] = 0.0;
      point[2] = center[2] - fj*sin(theta1);

      tex[0] = (mp->boardsize/2.0+point[0]) * mp->max_tx / mp->boardsize;
      tex[1] = (mp->boardsize/2.0+point[2]) * mp->max_ty / mp->boardsize;

      glTexCoord2f(tex[0], tex[1]);
      glVertex3f(point[0], point[1], point[2]);

      point[0] = center[0] + fj*cos(theta2);
      point[1] = 0.0;
      point[2] = center[2] - fj*sin(theta2);

      tex[0] = (mp->boardsize/2.0+point[0]) * mp->max_tx / mp->boardsize;
      tex[1] = (mp->boardsize/2.0+point[2]) * mp->max_ty / mp->boardsize;

      glTexCoord2f(tex[0], tex[1]);
      glVertex3f(point[0], point[1], point[2]);
      mi->polygon_count++;
    }

    glEnd();
  }

  glDisable(GL_TEXTURE_2D);
}

/* return euclidean distance between two points */
static double distance(double x[3], double y[3])
{
  double dx = x[0] - y[0];
  double dz = x[2] - y[2];
  return sqrt(dx*dx + dz*dz);
}

/* determine a new goal */
static void find_goal(antspotlightstruct *mp)
{
  do {
    mp->ant->goal[0] = random()%((int)(mp->boardsize+0.5)-2) - mp->boardsize/2.0 + 1.0;
    mp->ant->goal[1] = 0.0;
    mp->ant->goal[2] = random()%((int)(mp->boardsize+0.5)-2) - mp->boardsize/2.0 + 1.0;
  }
  while(distance(mp->ant->position, mp->ant->goal) < 2.0);
}

/* construct our ant */
static void build_ant(antspotlightstruct *mp)
{
  mp->ant = (Ant *) malloc(sizeof (Ant));
  mp->ant->position[0] = 0.0;
  mp->ant->position[1] = 0.0;
  mp->ant->position[2] = 0.0;
  mp->ant->direction = 0.0;
  mp->ant->velocity = 0.02;
  mp->ant->material = MaterialGray5;
  mp->ant->step = 0;
  find_goal(mp);
}

#define EPSILON 0.01

static double sign(double d)
{
  return d < 0.0 ? -1.0 : 1.0;
}

static double min(double a, double b)
{
  return a < b ? a : b;
}

/*
static double max(double a, double b)
{
  return a > b ? a : b;
}
*/

/* find a new goal and reset steps */
static void reset_ant(antspotlightstruct *mp)
{
  find_goal(mp);
}

/* draw ant composed of skeleton and glass */
static void show_ant(ModeInfo *mi, antspotlightstruct *mp)
{

  glPushMatrix();

  /* move into position */
  glTranslatef(mp->ant->position[0], 0.33, mp->ant->position[2]);
  glRotatef(180.0 + mp->ant->direction*180.0/Pi, 0.0, 1.0, 0.0);
  glRotatef(90.0, 0.0, 0.0, 1.0);

  /* draw skeleton */
  draw_ant(mi, mp, mp->ant->material, mp->mono, 0, mp->ant->step, mySphere2, myCone2);

  /* draw glass */
  if(!mp->wire && !mp->mono) {
    glEnable(GL_BLEND);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGrayB);
    glColor4fv(MaterialGrayB);
    draw_ant(mi, mp, MaterialGrayB, mp->mono, 0, mp->ant->step, mySphere, myCone2);
    glDisable(GL_BLEND);
  }

  glPopMatrix();
}

static void draw_antspotlight_strip(ModeInfo *mi)
{
  antspotlightstruct *mp = &antspotlight[MI_SCREEN(mi)];

  /* compute spotlight position and direction */
  GLfloat light1_position[4];

  light1_position[0] = mp->ant->position[0] + 0.7*cos(mp->ant->direction);
  light1_position[1] = 0.5;
  light1_position[2] = mp->ant->position[2] - 0.7*sin(mp->ant->direction);
  light1_position[3] = 1.0;

  mp->spot_direction[0] = cos(mp->ant->direction);
  mp->spot_direction[1] = -0.5;
  mp->spot_direction[2] = -sin(mp->ant->direction);

  glLightfv(GL_LIGHT2, GL_POSITION, light1_position);
  glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, mp->spot_direction);
  
  glEnable(GL_LIGHT2);
  glDisable(GL_LIGHT0);
  glDisable(GL_LIGHT1);

  /* draw board */
  if(mp->wire)
    ;
  else
    draw_board(mi, mp);

  glDisable(GL_LIGHT2);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  
  /* now modify ant */
  show_ant(mi, mp);

  /* near goal, bend path towards next step */
  if(distance(mp->ant->position, mp->ant->goal) < 0.2) {
    reset_ant(mp);
  }

  if(random()%100 == 0) {
    reset_ant(mp);
  }


  /* move toward goal, correct ant direction if required */
  else {
    
    /* difference */
    double dx = mp->ant->goal[0] - mp->ant->position[0];
    double dz = -(mp->ant->goal[2] - mp->ant->position[2]);
    double theta, ideal, dt;
    
    if(fabs(dx) > EPSILON) {
      theta = atan(dz/dx);
      if(dx < 0.0)
	theta += Pi;
    }
    else
      theta = dz > 0.0 ? (1.0/2.0)*Pi : (3.0/2.0)*Pi;
    
    if(theta < 0.0)
      theta += 2*Pi;
    
    ideal = theta - mp->ant->direction;
    if(ideal > Pi)
      ideal -= 2*Pi;
    
    /* compute correction */
    dt = sign(ideal) * min(fabs(ideal), Pi/100.0);
    mp->ant->direction += dt;
    while(mp->ant->direction < 0.0)
      mp->ant->direction += 2*Pi;
    while(mp->ant->direction > 2*Pi)
      mp->ant->direction -= 2*Pi;
  }

  mp->ant->position[0] += mp->ant->velocity * cos(mp->ant->direction);
  mp->ant->position[2] += mp->ant->velocity * sin(-mp->ant->direction);
  mp->ant->step += 10*mp->ant->velocity;
  while(mp->ant->step > 2*Pi)
    mp->ant->step -= 2*Pi;
}

ENTRYPOINT void reshape_antspotlight(ModeInfo * mi, int width, int height)
{
  double h = (GLfloat) height / (GLfloat) width;  
  int y = 0;
  int size = 2;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport(0, y, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective(45, 1/h, 1.0, 25.0);

  glMatrixMode(GL_MODELVIEW);
  glLineWidth(size);
  glPointSize(size);
}

/* lighting variables */
static const GLfloat front_shininess[] = {60.0};
static const GLfloat front_specular[] = {0.8, 0.8, 0.8, 1.0};
static const GLfloat ambient[] = {0.4, 0.4, 0.4, 1.0};
/*static const GLfloat ambient2[] = {0.0, 0.0, 0.0, 0.0};*/
static const GLfloat diffuse[] = {1.0, 1.0, 1.0, 1.0};
static const GLfloat position0[] = {1.0, 5.0, 1.0, 0.0};
static const GLfloat position1[] = {-1.0, -5.0, 1.0, 0.0};
/*static const GLfloat lmodel_ambient[] = {0.8, 0.8, 0.8, 1.0};*/
static const GLfloat lmodel_twoside[] = {GL_TRUE};
static const GLfloat spotlight_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
static const GLfloat spotlight_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };

static void pinit(void)
{
  glClearDepth(1.0);

  /* setup twoside lighting */
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, position0);
  glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT1, GL_POSITION, position1);

  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, spotlight_ambient);
  glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);

  /* setup spotlight */
  glLightfv(GL_LIGHT2, GL_AMBIENT, spotlight_ambient);
  glLightfv(GL_LIGHT2, GL_DIFFUSE, spotlight_diffuse);
  glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 0.1);
  glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.05);
  glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.0);
  glLightf(GL_LIGHT2, GL_SPOT_CUTOFF, 60.0);
  glLightf(GL_LIGHT2, GL_SPOT_EXPONENT, 3.0);

  glEnable(GL_NORMALIZE);
  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);

  /* setup material properties */
  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glShadeModel(GL_SMOOTH);
/*   glShadeModel(GL_FLAT); */
  glEnable(GL_DEPTH_TEST);
}

#define MAX_MAGNIFICATION 10
#define max(a, b) a < b ? b : a
#define min(a, b) a < b ? a : b

/* event handling */
ENTRYPOINT Bool antspotlight_handle_event(ModeInfo *mi, XEvent *event)
{
  antspotlightstruct *mp = &antspotlight[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, mp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &mp->button_down_p))
    return True;

  if (event->xany.type == ButtonPress)
    {
      switch(event->xbutton.button) {

      case Button1:
        mp->button_down_p = True;
        gltrackball_start(mp->trackball, 
                          event->xbutton.x, event->xbutton.y,
                          MI_WIDTH (mi), MI_HEIGHT (mi));
        return True;
      
      case Button4:
        mp->mag = max(mp->mag-1, 1);
        return True;

      case Button5:
        mp->mag = min(mp->mag+1, MAX_MAGNIFICATION);
        return True;
      }
    }

  return False;
}

static void
image_loaded_cb (const char *filename, XRectangle *geometry,
                 int image_width, int image_height, 
                 int texture_width, int texture_height,
                 void *closure)
{
  antspotlightstruct *mp = (antspotlightstruct *) closure;

  mp->max_tx = (GLfloat) image_width  / texture_width;
  mp->max_ty = (GLfloat) image_height / texture_height;

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   (mp->mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));

  mp->waiting_for_image_p = False;
}


/* get screenshot */
static void get_snapshot(ModeInfo *modeinfo)
{
  antspotlightstruct *mp = &antspotlight[MI_SCREEN(modeinfo)];

  if (MI_IS_WIREFRAME(modeinfo))
    return;

  mp->waiting_for_image_p = True;
  mp->mipmap_p = True;
  load_texture_async (modeinfo->xgwa.screen, modeinfo->window,
                      *mp->glx_context, 0, 0, mp->mipmap_p, 
                      mp->screentexture, image_loaded_cb, mp);
}


ENTRYPOINT void init_antspotlight(ModeInfo *mi)
{
  double rot_speed = 0.3;

  antspotlightstruct *mp;
  
  MI_INIT(mi, antspotlight);
  mp = &antspotlight[MI_SCREEN(mi)];
  mp->rot = make_rotator (rot_speed, rot_speed, rot_speed, 1, 0, True);
  mp->trackball = gltrackball_init (False);

  if((mp->glx_context = init_GL(mi)) != NULL) {
    reshape_antspotlight(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    glDrawBuffer(GL_BACK);
    pinit();
  }
  else
    MI_CLEARWINDOW(mi);

  glGenTextures(1, &mp->screentexture);
  glBindTexture(GL_TEXTURE_2D, mp->screentexture);
  get_snapshot(mi);

  build_ant(mp);
  mp->mono = MI_IS_MONO(mi);
  mp->wire = MI_IS_WIREFRAME(mi);
  mp->boardsize = 8.0;
  mp->mag = 1;
}

ENTRYPOINT void draw_antspotlight(ModeInfo * mi)
{
  antspotlightstruct *mp;
  
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  
  if(!antspotlight)
	return;
  mp = &antspotlight[MI_SCREEN(mi)];
  
  MI_IS_DRAWN(mi) = True;
  
  if(!mp->glx_context)
	return;
  
  mi->polygon_count = 0;

  /* Just keep running before the texture has come in. */
  /* if (mp->waiting_for_image_p) return; */

  glXMakeCurrent(display, window, *mp->glx_context);
  
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glPushMatrix();
  glRotatef(current_device_rotation(), 0, 0, 1);

  /* position camera */

  /* follow focused ant */
  glTranslatef(0.0, 0.0, -6.0 - mp->mag);
  glRotatef(35.0, 1.0, 0.0, 0.0);
  gltrackball_rotate(mp->trackball);
  glTranslatef(-mp->ant->position[0], mp->ant->position[1], -mp->ant->position[2]);

  /* stable position */
/*   glTranslatef(0.0, 0.0, -10.0 - mag); */
/*   gltrackball_rotate(mp->trackball); */
/*   glRotatef(40.0, 1.0, 0.0, 0.0); */
/*   glRotatef(20.0, 0.0, 1.0, 0.0); */

  draw_antspotlight_strip(mi);

  ++mp->ticks;
  
  glPopMatrix();
  
  if (MI_IS_FPS(mi)) do_fps (mi);
  glFlush();
  
  glXSwapBuffers(display, window);
}

#ifndef STANDALONE
ENTRYPOINT void change_antspotlight(ModeInfo * mi)
{
  antspotlightstruct *mp = &antspotlight[MI_SCREEN(mi)];
  
  if (!mp->glx_context)
	return;
  
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *mp->glx_context);
  pinit();
}
#endif /* !STANDALONE */

ENTRYPOINT void free_antspotlight(ModeInfo * mi)
{
  antspotlightstruct *mp = &antspotlight[MI_SCREEN(mi)];
  if (!mp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *mp->glx_context);
  gltrackball_free (mp->trackball);
  free_rotator (mp->rot);
  if (mp->screentexture) glDeleteTextures (1, &mp->screentexture);
}

XSCREENSAVER_MODULE ("AntSpotlight", antspotlight)
