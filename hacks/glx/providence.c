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
 */

#ifdef STANDALONE
#define DEFAULTS	    "*delay:   20000   \n" \
			    "*showFPS: False   \n" \
			    "*wireframe: False \n"

# define refresh_providence 0
#include "xlockmore.h"
#else
#include "xlock.h"
#endif

#include "gltrackball.h"

#define DEF_SOLIDPROVIDENCE  "False"
#define DEF_EYE  "True"

static int eye;

static XrmOptionDescRec opts[] = {
  {"-eye", ".providence.eye", XrmoptionNoArg, "on"},
  {"+eye", ".providence.eye", XrmoptionNoArg, "off"}
};

static argtype vars[] = {
  {&eye, "eye", "Eye", DEF_EYE, t_Bool}
};

static OptionStruct desc[] = {
  {"-/+eye", "turn on/off eye of providence"}
};

ENTRYPOINT ModeSpecOpt providence_opts = {
  sizeof opts / sizeof opts[0], opts, 
  sizeof vars / sizeof vars[0], vars, desc
};

#ifdef USE_MODULES
ModStruct   providence_description = {
  "providence", "init_providence", "draw_providence", 
  "release_providence", "draw_providence", "change_providence", 
  (char *) NULL, &providence_opts, 1000, 1, 1, 1, 4, 1.0, "",
  "draws pyramid with glory", 0, NULL
};
#endif

#define Scale4Window               0.3
#define Scale4Iconic               0.4

#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi M_PI
#endif

/* brick texture */
#define checkImageWidth 64
#define checkImageHeight 64

#define EYE_PARTICLE_COUNT 2000

#define LOOKUPSIZE 3600
#define EYELENGTH 300

#define EPSILON 0.0001
#define PARTICLE_COUNT 2000
#define FPS 50


typedef struct {
  GLint       WindH, WindW;
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool        button_down_p;
  GLfloat position0[4];
  GLubyte checkImage[checkImageWidth][checkImageHeight][3];
  GLuint bricktexture;

  int mono, wire;
  double camera_velocity;
  double camera_z;

  int pyramidlist;
  double currenttime;
  double theta;
  double theta_scale;

  double particles[PARTICLE_COUNT][5];
  int eyeparticles[EYE_PARTICLE_COUNT][2];
  double lookup[LOOKUPSIZE][EYELENGTH][2];
  double lookup2[LOOKUPSIZE][EYELENGTH][2];

} providencestruct;

/* lighting variables */
/*static const GLfloat front_shininess[] = {60.0};*/
/*static const GLfloat front_specular[] = {0.2, 0.2, 0.2, 1.0};*/
/*static const GLfloat ambient[] = {0.8, 0.8, 0.8, 1.0};*/
static const GLfloat ambient2[] = {0.25, 0.25, 0.25, 1.0};
static const GLfloat diffuse[] = {1.0, 1.0, 1.0, 1.0};
static const GLfloat lmodel_ambient[] = {0.5, 0.5, 0.5, 1.0};
static const GLfloat lmodel_twoside[] = {GL_TRUE};

/* gray-gray */

static const GLfloat MaterialGlory[] = {0.04, 0.30, 0.22, 0.7};
static const GLfloat MaterialGloryB[] = {0.07, 0.50, 0.36, 0.6};

static const GLfloat MaterialGloryF[] = {0.07, 0.50, 0.36, 1.0};
/* static const GLfloat MaterialGloryF[] = {0.06, 0.38, 0.27, 1.0}; */
/*static const GLfloat MaterialGloryE[] = {0.06, 0.38, 0.27, 0.3};*/
static const GLfloat MaterialGloryM[] = {0.5, 0.5, 0.5, 0.5};
static const GLfloat MaterialGloryMB[] = {0.36, 0.36, 0.36, 0.4};
/*static const GLfloat MaterialGreenback[4] = {0.04, 0.30, 0.22, 1.0};*/
/*static const GLfloat MaterialBlack[4] = {0.0, 0.0, 0.0, 1.0};*/

static const GLfloat MaterialGray5[] = {0.5, 0.5, 0.5, 1.0};
/*static const GLfloat MaterialGray6[] = {0.6, 0.6, 0.6, 1.0};*/

static providencestruct *providence = (providencestruct *) NULL;

#define NUM_SCENES      2

/* build brick texture */
static void make_brick(providencestruct *mp) 
{
  int i, j, c;

  for (i = 0; i < checkImageWidth; i++) {
    for (j = 0; j < checkImageHeight; j++) {
      c = i % 16 == 15 ? 255 : (j + 48*(i / 16))%64 == 0 ? 255 : 
	102 + random() % 102;
      mp->checkImage[i][j][0] = (GLubyte) c;
      mp->checkImage[i][j][1] = (GLubyte) c;
      mp->checkImage[i][j][2] = (GLubyte) c;
    }
  }

  glGenTextures(1, &mp->bricktexture);
  glBindTexture(GL_TEXTURE_2D, mp->bricktexture);

  glTexImage2D(GL_TEXTURE_2D, 0, 3, checkImageWidth, 
	       checkImageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 
	       &mp->checkImage[0][0]);
}


/* build eye lookup table */
static void build_eye(providencestruct *mp) 
{
  int i, j;
  double x;
  double inc = 0.1 / EYELENGTH;
  double inc2 = 2.4*Pi / EYELENGTH;

  /* describe all values tangentially out from pupil */
  for(i = 0; i < LOOKUPSIZE; ++i) {
    double r = i * 2*Pi / LOOKUPSIZE;/*x + inc;*/
    double sr = sin(r);
    double cr = cos(r);
    x = 0.07;

    for(j = 0; j < EYELENGTH; ++j) {
      mp->lookup[i][j][0] = x*sr;
      mp->lookup[i][j][1] = x*cr;
      x += inc;
    }
  }

  /* lookup2: dollar sign */
  for(i = 0; i < LOOKUPSIZE; ++i) {
    double y = -1.2*Pi;
    
    for(j = 0; j < EYELENGTH; ++j) {
      if(i % 2) {
	mp->lookup2[i][j][0] = sin(y)/6.0 + i/36000.0 - 0.05;
	mp->lookup2[i][j][1] = i%4 ? y/12.0 - 0.05 : 1.2*Pi-y/12.0 + 0.05;
      }
      else {
	mp->lookup2[i][j][0] = i/36000.0 - 0.05;
	mp->lookup2[i][j][1] = y/9.0 - 0.05;
      }
      y += inc2;
    }
  }
}


static double min(double a, double b) 
{
  return a < b ? a : b;
}

static double max(double a, double b) 
{
  return a > b ? a : b;
}

static void init_particle(providencestruct *mp, double particle[5]) 
{
  /* position along glory */
  double p = (random() % 485410) / 100000.0;

  /* on a plane */
  particle[2] = 0.0;
  
  if(p < 1.5) {
    particle[0] = p - 0.75;
    particle[1] = -0.75001;
  }
  else if(p < 1.5 + sqrt(45)/4.0) {
    double d = p - 1.5;
    particle[0] = 0.75 - d*cos(atan(2.0));
    particle[1] = d*sin(atan(2.0)) - 0.75;
  }
  else {
    double d = 4.8541 - p;
    particle[0] = -0.75 + d*cos(atan(2.0));
    particle[1] = d*sin(atan(2.0)) - 0.75;
  }

  particle[3] = mp->currenttime;
  particle[4] = 1.25 + (random()%10)/10.0;
}

/* init glory particles */
static void init_particles(providencestruct *mp) 
{
  int i;
  for(i = 0; i < PARTICLE_COUNT; ++i) {
    init_particle(mp, mp->particles[i]);

    /* set initial time */
    mp->particles[i][3] = mp->currenttime - (random()%1250)/1000.0;
  }

  /* init eye particles */
  for(i = 0; i < EYE_PARTICLE_COUNT; ++i) {
    mp->eyeparticles[i][0] = random()%LOOKUPSIZE;
    mp->eyeparticles[i][1] = random()%EYELENGTH;
  }
}


/* ugg, should be a priority queue if next event times known */
static void update_particles(providencestruct *mp) 
{
  int i;

  for(i = 0; i < PARTICLE_COUNT; ++i) {
    /* check for time elapse */
    if(mp->currenttime > mp->particles[i][3] + mp->particles[i][4])
      init_particle(mp, mp->particles[i]);
  }

  /* now update eye particles */
  for(i = 0; i < EYE_PARTICLE_COUNT; ++i) {
/*     int x = eyeparticles[i][1] + random()%16; */
    int x = mp->eyeparticles[i][1] + random()%(cos(mp->theta) < 0.0 ? 8 : 16);

    /* reset if dead */
    if(x > EYELENGTH || random()%(cos(mp->theta) < 0.0 ? 40 : 10) == 0) {

/*     if(x > EYELENGTH || (x > EYELENGTH/(2/3.0) && random()%7 == 0)) { */
      mp->eyeparticles[i][0] = random()%LOOKUPSIZE;
      mp->eyeparticles[i][1] = random()%40;
    }
    else {
      mp->eyeparticles[i][1] = x;
    }    
  }
}

/* draw the pyramid */
static void draw_seal(providencestruct *mp) 
{
  int i;
  double base = sqrt(2.0);
  double top = 1.0 / sqrt(2.0);
  double tmod = 7.0/6.0;

  glPushMatrix();

  /* set options for mono, wireframe */
  if(mp->wire) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
  }
  else {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, mp->bricktexture);

    glEnable(GL_LIGHTING);

    glColor4fv(mp->mono ? MaterialGray5 : MaterialGloryF);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
		 mp->mono ? MaterialGray5 : MaterialGloryF);
  }

  glRotatef(45.0, 0.0, 1.0, 0.0);
  glTranslatef(0.0, -3.25, 0.0);

  for(i = 0; i < 4; ++i) {
    glRotatef(i*90.0, 0.0, 1.0, 0.0);

    glBegin(GL_QUADS);
    glNormal3f(1 / sqrt(6.0), 2 / sqrt(6.0), 1 / sqrt(6.0));
    glTexCoord2f(-base, 0.0);
    glVertex3f(-base, 0.0, base);
    glTexCoord2f(base, 0.0);
    glVertex3f(base, 0.0, base);
    glTexCoord2f(top, 13.0/4.0);
    glVertex3f(top, 2.0, top);
    glTexCoord2f(-top, 13.0/4.0);
    glVertex3f(-top, 2.0, top);
    glEnd();
  }

  glBegin(GL_QUADS);

  /* top */
  glNormal3f(0.0, 1.0, 0.0);
  glTexCoord2f(0.02, 0.0);
  glVertex3f(-top, 2.0, top);
  glTexCoord2f(2.0*top, 0.0);
  glVertex3f(top, 2.0, top);
  glTexCoord2f(2.0*top, tmod*2.1*top);
  glVertex3f(top, 2.0, -top);
  glTexCoord2f(0.02, tmod*2.1*top);
  glVertex3f(-top, 2.0, -top);

  /* base */
  glNormal3f(0.0, -1.0, 0.0);
  glTexCoord2f(-base, 0.0);
  glVertex3f(-base, 0.0, -base);
  glTexCoord2f(top, 0.0);
  glVertex3f(base, 0.0, -base);
  glTexCoord2f(top, top*13.0/4.0);
  glVertex3f(base, 0.0, base);
  glTexCoord2f(-top, top*13.0/4.0);
  glVertex3f(-base, 0.0, base);

  glEnd();

  glPopMatrix();
  glDisable(GL_TEXTURE_2D);
}

/* draw glory */
static void draw_glory(providencestruct *mp) 
{
  int i;

  if(mp->wire) {
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.75, -0.75, 0.0);
    glVertex3f(0.75, -0.75, 0.0);
    glVertex3f(0.0, 0.75, 0.0);

    glVertex3f(0.0, 0.75, 0.0);
    glVertex3f(0.75, -0.75, 0.0);
    glVertex3f(-0.75, -0.75, 0.0);
    glEnd();
    return;
  }

  /* draw particles */
  glDisable(GL_LIGHTING);
  glPushMatrix();
  glEnable(GL_BLEND);

  /* glory colour lines */
  glColor4fv(mp->mono ? MaterialGloryM : MaterialGlory);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mp->mono ? MaterialGloryM : MaterialGlory);

  glBegin(GL_LINES);
  for(i = 0; i < PARTICLE_COUNT/2; ++i) {
    double t = mp->currenttime - mp->particles[i][3];
    double th = atan(mp->particles[i][1] / mp->particles[i][0]);
    if(mp->particles[i][0] < 0.0)
      th += Pi;

    glVertex3f(mp->particles[i][0], mp->particles[i][1], mp->particles[i][2]);
    glVertex3f(mp->particles[i][0] + 0.2*cos(th)*t,
	       mp->particles[i][1] + 0.2*sin(th)*t,
	       mp->particles[i][2]);
  }
  glEnd();
  
  /* gloryb colour lines */
  glColor4fv(mp->mono ? MaterialGloryMB : MaterialGloryB);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mp->mono ? MaterialGloryMB : MaterialGloryB);
  glBegin(GL_LINES);
  for(; i < PARTICLE_COUNT; ++i) {
    double t = mp->currenttime - mp->particles[i][3];
    double th = atan(mp->particles[i][1] / mp->particles[i][0]);
    if(mp->particles[i][0] < 0.0)
      th += Pi;

    glVertex3f(mp->particles[i][0], mp->particles[i][1], mp->particles[i][2]);
    glVertex3f(mp->particles[i][0] + 0.2*cos(th)*t,
	       mp->particles[i][1] + 0.2*sin(th)*t,
	       mp->particles[i][2]);
  }
  glEnd();

  glPopMatrix();
  glEnable(GL_LIGHTING);
}

/* draw eye of providence */
static void draw_eye(providencestruct *mp) 
{
  int i;

  /* draw wireeye */
  if(mp->wire) {
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.25, -0.25, 0.0);
    glVertex3f(0.25, -0.25, 0.0);
    glVertex3f(0.0, 0.25, 0.0);
    glEnd();
    return;
  }

  /* draw particles */
  glDisable(GL_LIGHTING);
  glPushMatrix();
  glEnable(GL_BLEND);

  /* eye */
  glColor4fv(mp->mono ? MaterialGloryM : MaterialGlory);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mp->mono ? MaterialGloryM : MaterialGlory);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(i = 0; i < EYE_PARTICLE_COUNT/2; ++i) {
    glVertex3f(mp->lookup[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][0], 
	       mp->lookup[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  /* eye */
  glColor4fv(mp->mono ? MaterialGloryMB : MaterialGloryB);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mp->mono ? MaterialGloryMB : MaterialGloryB);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(; i < EYE_PARTICLE_COUNT; ++i) {
    glVertex3f(mp->lookup[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][0], 
	       mp->lookup[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();


  /* draw scaled particles */
  glPushMatrix();
  glScalef(3.3, 2.2, 3.3);

  /* eye */
  glColor4fv(mp->mono ? MaterialGloryMB : MaterialGloryB);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mp->mono ? MaterialGloryMB : MaterialGloryB);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(i = 0; i < EYE_PARTICLE_COUNT/2; ++i) {
    glVertex3f(mp->lookup[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][0], 
	       mp->lookup[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  glColor4fv(mp->mono ? MaterialGloryM : MaterialGlory);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mp->mono ? MaterialGloryM : MaterialGlory);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(; i < EYE_PARTICLE_COUNT; ++i) {
    glVertex3f(mp->lookup[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][0], 
	       mp->lookup[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  glPopMatrix();

  glPopMatrix();
  glEnable(GL_LIGHTING);
}

/* draw eye of providence */
static void draw_eye2(providencestruct *mp) 
{
  int i;

  /* draw wireeye */
  if(mp->wire) {
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0, 0.25, 0.0);
    glVertex3f(0.25, -0.25, 0.0);
    glVertex3f(-0.25, -0.25, 0.0);
    glEnd();
    return;
  }

  /* draw particles */
  glDisable(GL_LIGHTING);
  glPushMatrix();
  glEnable(GL_BLEND);

  /* eye */
  glColor4fv(mp->mono ? MaterialGloryM : MaterialGlory);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mp->mono ? MaterialGloryM : MaterialGlory);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(i = 0; i < EYE_PARTICLE_COUNT/2; ++i) {
    glVertex3f(mp->lookup2[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][0], 
	       mp->lookup2[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  /* eye */
  glColor4fv(mp->mono ? MaterialGloryMB : MaterialGloryB);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mp->mono ? MaterialGloryMB : MaterialGloryB);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(; i < EYE_PARTICLE_COUNT; ++i) {
    glVertex3f(mp->lookup2[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][0], 
	       mp->lookup2[mp->eyeparticles[i][0]][mp->eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  glPopMatrix();
  glEnable(GL_LIGHTING);
}

/* draw the scene */
static void draw_providence_strip(ModeInfo *mi) 
{
  providencestruct *mp = &providence[MI_SCREEN(mi)];
  glTranslatef(0.0, 1.414, 0.0);

  mp->position0[0] = 1.6*sin(mp->theta);
  mp->position0[1] = 1.2;
  mp->position0[2] = 1.6*cos(mp->theta);
  mp->position0[3] = 0.0;
  glLightfv(GL_LIGHT0, GL_POSITION, mp->position0);
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient2);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  /* draw pyramid, glory */
  glDisable(GL_BLEND);
  glCallList(mp->pyramidlist);
  draw_glory(mp);
  if(eye) {
    if(cos(mp->theta) < 0.0)
      draw_eye2(mp);
    else
      draw_eye(mp);
  }

  return;
}

ENTRYPOINT void reshape_providence(ModeInfo * mi, int width, int height) 
{
  double h = (GLfloat) height / (GLfloat) width;  
  providencestruct *mp = &providence[MI_SCREEN(mi)];

  glViewport(0, 0, mp->WindW = (GLint) width, mp->WindH = (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective(45, 1/h, 0.001, 25.0);

  glMatrixMode(GL_MODELVIEW);
  glLineWidth(2.0);
  glPointSize(2.0);
}

static void pinit(providencestruct *mp) 
{
  glClearDepth(1.0);

  mp->currenttime = 0.0;
  init_particles(mp);
  make_brick(mp);
  build_eye(mp);

  /* build pyramid list */
  mp->pyramidlist = glGenLists(1);
  glNewList(mp->pyramidlist, GL_COMPILE);
  draw_seal(mp);
  glEndList();
}

/* cleanup routine */
ENTRYPOINT void release_providence(ModeInfo * mi) 
{

  if(providence) {
    free((void *) providence);
    providence = (providencestruct *) NULL;
  }

  FreeAllGL(mi);
}

/* event handling */
ENTRYPOINT Bool providence_handle_event(ModeInfo *mi, XEvent *event) 
{
  providencestruct *mp = &providence[MI_SCREEN(mi)];

  switch(event->xany.type) {
  case ButtonPress:

    switch(event->xbutton.button) {

    case Button1:
      mp->button_down_p = True;
      gltrackball_start(mp->trackball, 
			event->xbutton.x, event->xbutton.y,
			MI_WIDTH (mi), MI_HEIGHT (mi));
      break;
      
    case Button4:
      mp->camera_velocity += 1.0;
      break;

    case Button5:
      mp->camera_velocity -= 1.0;
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

ENTRYPOINT void init_providence(ModeInfo *mi) 
{
  providencestruct *mp;
  
  if(!providence) {
    if((providence = (providencestruct *) 
	calloc(MI_NUM_SCREENS(mi), sizeof (providencestruct))) == NULL)
      return;
  }
  mp = &providence[MI_SCREEN(mi)];
  mp->trackball = gltrackball_init ();

  mp->position0[0] = 1;
  mp->position0[1] = 5;
  mp->position0[2] = 1;
  mp->position0[3] = 1;

  mp->camera_velocity = -8.0;

  mp->mono = MI_IS_MONO(mi);
  mp->wire = MI_IS_WIREFRAME(mi);

  /* make multiple screens rotate at slightly different rates. */
  mp->theta_scale = 0.7 + frand(0.6);

  if((mp->glx_context = init_GL(mi)) != NULL) {
    reshape_providence(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    /* glDrawBuffer(GL_BACK); */
    pinit(mp);
  }
  else
    MI_CLEARWINDOW(mi);
}

ENTRYPOINT void draw_providence(ModeInfo * mi) 
{
  providencestruct *mp;
  
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  
  if(!providence)
    return;
  mp = &providence[MI_SCREEN(mi)];
  
  MI_IS_DRAWN(mi) = True;
  
  if(!mp->glx_context)
    return;
  
  glXMakeCurrent(display, window, *(mp->glx_context));
  
  /* setup twoside lighting */
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient2);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, mp->position0);

  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
  glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  glEnable(GL_NORMALIZE);
  glFrontFace(GL_CCW);
/*   glDisable(GL_CULL_FACE); */
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glShadeModel(GL_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  glRotatef(current_device_rotation(), 0, 0, 1);

  /* modify camera */
  if(fabs(mp->camera_velocity) > EPSILON) {
    mp->camera_z = max(min(mp->camera_z + 0.1*mp->camera_velocity, -4.0), -12.0);
    mp->camera_velocity = 0.95*mp->camera_velocity;
  }
  
  /* rotate providence */
  glTranslatef(0.0, 0.0, mp->camera_z + sin(mp->theta/4.0));
  glRotatef(10.0+20.0*sin(mp->theta/2.0), 1.0, 0.0, 0.0);
  gltrackball_rotate(mp->trackball);
  glRotatef(mp->theta * 180.0 / Pi, 0.0, -1.0, 0.0);

  /* draw providence */
  draw_providence_strip(mi);
  glPopMatrix();
  
  if(MI_IS_FPS(mi)) do_fps (mi);
  glFlush();
  
  glXSwapBuffers(display, window);

  /* update */
  mp->currenttime += 1.0 / FPS;
  mp->theta = mp->currenttime / 2.0 * mp->theta_scale;
  update_particles(mp);
}

#ifndef STANDALONE
ENTRYPOINT void change_providence(ModeInfo * mi) 
{
  providencestruct *mp = &providence[MI_SCREEN(mi)];
  
  if (!mp->glx_context)
	return;
  
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(mp->glx_context));
  pinit();
}
#endif /* !STANDALONE */

XSCREENSAVER_MODULE ("Providence", providence)
