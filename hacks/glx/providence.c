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

#include <X11/Intrinsic.h>

#ifdef STANDALONE
#define PROGCLASS	    "Providence"
#define HACK_INIT	    init_providence
#define HACK_DRAW	    draw_providence
#define HACK_RESHAPE	    reshape_providence
#define HACK_HANDLE_EVENT   providence_handle_event
#define EVENT_MASK	    PointerMotionMask
#define providence_opts	    xlockmore_opts
#define DEFAULTS	    "*delay:   20000   \n" \
			    "*showFPS: False   \n" \
			    "*wireframe: False \n"

#include "xlockmore.h"
#else
#include "xlock.h"
#endif

#include <GL/glu.h>
#include "gltrackball.h"

#define DEF_SOLIDPROVIDENCE  "False"
#define DEF_EYE  "True"

static int eye;

static XrmOptionDescRec opts[] = {
  {(char *) "-eye", 
   (char *) ".providence.eye", XrmoptionNoArg, (caddr_t) "on"},

  {(char *) "+eye", 
   (char *) ".providence.eye", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] = {
  {(caddr_t *) &eye, 
   (char *) "eye", (char *) "Eye", (char *) DEF_EYE, t_Bool}
};

static OptionStruct desc[] = {
  {(char *) "-/+eye", 
   (char *) "turn on/off eye of providence"}
};

ModeSpecOpt providence_opts = {
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

int mono = 0, wire = 0;
double camera_velocity = 0.0;
double camera_z = -8.0;
int camera_mode = 0;

typedef struct {
  GLint       WindH, WindW;
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool        button_down_p;
} providencestruct;

/* lighting variables */
GLfloat front_shininess[] = {60.0};
GLfloat front_specular[] = {0.2, 0.2, 0.2, 1.0};
GLfloat ambient[] = {0.8, 0.8, 0.8, 1.0};
GLfloat ambient2[] = {0.25, 0.25, 0.25, 1.0};
GLfloat diffuse[] = {1.0, 1.0, 1.0, 1.0};
GLfloat position0[] = {1.0, 5.0, 1.0, 1.0};
GLfloat position1[] = {-1.0, -5.0, 1.0, 1.0};
GLfloat lmodel_ambient[] = {0.5, 0.5, 0.5, 1.0};
GLfloat lmodel_twoside[] = {GL_TRUE};

/* gray-gray */

GLfloat MaterialGlory[] = {0.04, 0.30, 0.22, 0.7};
GLfloat MaterialGloryB[] = {0.07, 0.50, 0.36, 0.6};

GLfloat MaterialGloryF[] = {0.07, 0.50, 0.36, 1.0};
/* GLfloat MaterialGloryF[] = {0.06, 0.38, 0.27, 1.0}; */
GLfloat MaterialGloryE[] = {0.06, 0.38, 0.27, 0.3};
GLfloat MaterialGloryM[] = {0.5, 0.5, 0.5, 0.5};
GLfloat MaterialGloryMB[] = {0.36, 0.36, 0.36, 0.4};
GLfloat MaterialGreenback[4] = {0.04, 0.30, 0.22, 1.0};
GLfloat MaterialBlack[4] = {0.0, 0.0, 0.0, 1.0};

GLfloat MaterialGray5[] = {0.5, 0.5, 0.5, 1.0};
GLfloat MaterialGray6[] = {0.6, 0.6, 0.6, 1.0};

double currenttime;

static providencestruct *providence = (providencestruct *) NULL;

#define NUM_SCENES      2

/* brick texture */
#define checkImageWidth 64
#define checkImageHeight 64
GLubyte checkImage[checkImageWidth][checkImageHeight][3];
GLuint bricktexture;

/* build brick texture */
void make_brick(void) {
  int i, j, c;

  for (i = 0; i < checkImageWidth; i++) {
    for (j = 0; j < checkImageHeight; j++) {
      c = i % 16 == 15 ? 255 : (j + 48*(i / 16))%64 == 0 ? 255 : 
	102 + random() % 102;
      checkImage[i][j][0] = (GLubyte) c;
      checkImage[i][j][1] = (GLubyte) c;
      checkImage[i][j][2] = (GLubyte) c;
    }
  }

  glGenTextures(1, &bricktexture);
  glBindTexture(GL_TEXTURE_2D, bricktexture);

  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, 3, checkImageWidth, 
	       checkImageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 
	       &checkImage[0][0]);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/* int min */
int mini(int a, int b) { return a < b ? a : b; }

/* eye particles */
#define EYE_PARTICLE_COUNT 2000
int eyeparticles[EYE_PARTICLE_COUNT][2];

/* lookup table for the eye */
#define LOOKUPSIZE 3600
#define EYELENGTH 300
double lookup[LOOKUPSIZE][EYELENGTH][2];
double lookup2[LOOKUPSIZE][EYELENGTH][2];

/* build eye lookup table */
void build_eye(void) {
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
      lookup[i][j][0] = x*sr;
      lookup[i][j][1] = x*cr;
      x += inc;
    }
  }

  /* lookup2: dollar sign */
  for(i = 0; i < LOOKUPSIZE; ++i) {
    double y = -1.2*Pi;
    
    for(j = 0; j < EYELENGTH; ++j) {
      if(i % 2) {
	lookup2[i][j][0] = sin(y)/6.0 + i/36000.0 - 0.05;
	lookup2[i][j][1] = i%4 ? y/12.0 - 0.05 : 1.2*Pi-y/12.0 + 0.05;
      }
      else {
	lookup2[i][j][0] = i/36000.0 - 0.05;
	lookup2[i][j][1] = y/9.0 - 0.05;
      }
      y += inc2;
    }
  }
}

int pyramidlist;
#define EPSILON 0.0001

double min(double a, double b) {
  return a < b ? a : b;
}

double max(double a, double b) {
  return a > b ? a : b;
}

#define PARTICLE_COUNT 2000
double particles[PARTICLE_COUNT][5];

void init_particle(double particle[5]) {
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

  particle[3] = currenttime;
  particle[4] = 1.25 + (random()%10)/10.0;
}

/* init glory particles */
void init_particles(void) {
  int i;
  for(i = 0; i < PARTICLE_COUNT; ++i) {
    init_particle(particles[i]);

    /* set initial time */
    particles[i][3] = currenttime - (random()%1250)/1000.0;
  }

  /* init eye particles */
  for(i = 0; i < EYE_PARTICLE_COUNT; ++i) {
    eyeparticles[i][0] = random()%LOOKUPSIZE;
    eyeparticles[i][1] = random()%EYELENGTH;
  }
}

#define FPS 50
double theta = 0.0;

/* ugg, should be a priority queue if next event times known */
void update_particles(void) {
  int i;

  for(i = 0; i < PARTICLE_COUNT; ++i) {
    /* check for time elapse */
    if(currenttime > particles[i][3] + particles[i][4])
      init_particle(particles[i]);
  }

  /* now update eye particles */
  for(i = 0; i < EYE_PARTICLE_COUNT; ++i) {
/*     int x = eyeparticles[i][1] + random()%16; */
    int x = eyeparticles[i][1] + random()%(cos(theta) < 0.0 ? 8 : 16);

    /* reset if dead */
    if(x > EYELENGTH || random()%(cos(theta) < 0.0 ? 40 : 10) == 0) {

/*     if(x > EYELENGTH || (x > EYELENGTH/(2/3.0) && random()%7 == 0)) { */
      eyeparticles[i][0] = random()%LOOKUPSIZE;
      eyeparticles[i][1] = random()%40;
    }
    else {
      eyeparticles[i][1] = x;
    }    
  }
}

/* draw the pyramid */
void draw_seal(void) {
  int i;
  double base = sqrt(2.0);
  double top = 1.0 / sqrt(2.0);
  double tmod = 7.0/6.0;

  glPushMatrix();

  /* set options for mono, wireframe */
  if(wire) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
  }
  else {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, bricktexture);

    glEnable(GL_LIGHTING);

    glColor4fv(mono ? MaterialGray5 : MaterialGloryF);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
		 mono ? MaterialGray5 : MaterialGloryF);
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
void draw_glory(void) {
  int i;

  if(wire) {
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.75, -0.75, 0.0);
    glVertex3f(0.75, -0.75, 0.0);
    glVertex3f(0.0, 0.75, 0.0);
    glEnd();
    return;
  }

  /* draw particles */
  glDisable(GL_LIGHTING);
  glPushMatrix();
  glEnable(GL_BLEND);

  /* glory colour lines */
  glColor4fv(mono ? MaterialGloryM : MaterialGlory);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mono ? MaterialGloryM : MaterialGlory);

  glBegin(GL_LINES);
  for(i = 0; i < PARTICLE_COUNT/2; ++i) {
    double t = currenttime - particles[i][3];
    double th = atan(particles[i][1] / particles[i][0]);
    if(particles[i][0] < 0.0)
      th += Pi;

    glVertex3f(particles[i][0], particles[i][1], particles[i][2]);
    glVertex3f(particles[i][0] + 0.2*cos(th)*t,
	       particles[i][1] + 0.2*sin(th)*t,
	       particles[i][2]);
  }
  glEnd();
  
  /* gloryb colour lines */
  glColor4fv(mono ? MaterialGloryMB : MaterialGloryB);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mono ? MaterialGloryMB : MaterialGloryB);
  glBegin(GL_LINES);
  for(; i < PARTICLE_COUNT; ++i) {
    double t = currenttime - particles[i][3];
    double th = atan(particles[i][1] / particles[i][0]);
    if(particles[i][0] < 0.0)
      th += Pi;

    glVertex3f(particles[i][0], particles[i][1], particles[i][2]);
    glVertex3f(particles[i][0] + 0.2*cos(th)*t,
	       particles[i][1] + 0.2*sin(th)*t,
	       particles[i][2]);
  }
  glEnd();

  glPopMatrix();
  glEnable(GL_LIGHTING);
}

/* draw eye of providence */
void draw_eye(void) {
  int i;

  /* draw wireeye */
  if(wire) {
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
  glColor4fv(mono ? MaterialGloryM : MaterialGlory);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mono ? MaterialGloryM : MaterialGlory);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(i = 0; i < EYE_PARTICLE_COUNT/2; ++i) {
    glVertex3f(lookup[eyeparticles[i][0]][eyeparticles[i][1]][0], 
	       lookup[eyeparticles[i][0]][eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  /* eye */
  glColor4fv(mono ? MaterialGloryMB : MaterialGloryB);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mono ? MaterialGloryMB : MaterialGloryB);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(; i < EYE_PARTICLE_COUNT; ++i) {
    glVertex3f(lookup[eyeparticles[i][0]][eyeparticles[i][1]][0], 
	       lookup[eyeparticles[i][0]][eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();


  /* draw scaled particles */
  glPushMatrix();
  glScalef(3.3, 2.2, 3.3);

  /* eye */
  glColor4fv(mono ? MaterialGloryMB : MaterialGloryB);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mono ? MaterialGloryMB : MaterialGloryB);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(i = 0; i < EYE_PARTICLE_COUNT/2; ++i) {
    glVertex3f(lookup[eyeparticles[i][0]][eyeparticles[i][1]][0], 
	       lookup[eyeparticles[i][0]][eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  glColor4fv(mono ? MaterialGloryM : MaterialGlory);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mono ? MaterialGloryM : MaterialGlory);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(; i < EYE_PARTICLE_COUNT; ++i) {
    glVertex3f(lookup[eyeparticles[i][0]][eyeparticles[i][1]][0], 
	       lookup[eyeparticles[i][0]][eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  glPopMatrix();

  glPopMatrix();
  glEnable(GL_LIGHTING);
}

/* draw eye of providence */
void draw_eye2(void) {
  int i;

  /* draw wireeye */
  if(wire) {
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
  glColor4fv(mono ? MaterialGloryM : MaterialGlory);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mono ? MaterialGloryM : MaterialGlory);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(i = 0; i < EYE_PARTICLE_COUNT/2; ++i) {
    glVertex3f(lookup2[eyeparticles[i][0]][eyeparticles[i][1]][0], 
	       lookup2[eyeparticles[i][0]][eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  /* eye */
  glColor4fv(mono ? MaterialGloryMB : MaterialGloryB);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
	       mono ? MaterialGloryMB : MaterialGloryB);

  /* draw eye particles on z = 0 plane */
  glBegin(GL_POINTS);
  for(; i < EYE_PARTICLE_COUNT; ++i) {
    glVertex3f(lookup2[eyeparticles[i][0]][eyeparticles[i][1]][0], 
	       lookup2[eyeparticles[i][0]][eyeparticles[i][1]][1],
	       0.0);
  }
  glEnd();

  glPopMatrix();
  glEnable(GL_LIGHTING);
}

/* draw the scene */
void draw_providence_strip(ModeInfo *mi) {
  glTranslatef(0.0, 1.414, 0.0);

  position0[0] = 1.6*sin(theta);
  position0[1] = 1.2;
  position0[2] = 1.6*cos(theta);
  position0[3] = 0.0;
  glLightfv(GL_LIGHT0, GL_POSITION, position0);
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient2);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  /* draw pyramid, glory */
  glDisable(GL_BLEND);
  glCallList(pyramidlist);
  draw_glory();
  if(eye) {
    if(cos(theta) < 0.0)
      draw_eye2();
    else
      draw_eye();
  }

  return;
}

void reshape_providence(ModeInfo * mi, int width, int height) {
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

static void pinit(void) {
  glClearDepth(1.0);
  glClearColor(0.0, 0.0, 0.0, 1.0);
  
  /* setup twoside lighting */
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient2);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, position0);

  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
  glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  currenttime = 0.0;
  init_particles();
  make_brick();
  build_eye();

  glEnable(GL_NORMALIZE);
  glFrontFace(GL_CCW);
/*   glDisable(GL_CULL_FACE); */
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glShadeModel(GL_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  /* build pyramid list */
  pyramidlist = glGenLists(1);
  glNewList(pyramidlist, GL_COMPILE);
  draw_seal();
  glEndList();
}

/* cleanup routine */
void release_providence(ModeInfo * mi) {

  if(providence) {
    free((void *) providence);
    providence = (providencestruct *) NULL;
  }

  FreeAllGL(mi);
}

/* event handling */
Bool providence_handle_event(ModeInfo *mi, XEvent *event) {
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
      camera_velocity += 1.0;
      break;

    case Button5:
      camera_velocity -= 1.0;
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

void init_providence(ModeInfo *mi) {
  providencestruct *mp;
  
  if(!providence) {
    if((providence = (providencestruct *) 
	calloc(MI_NUM_SCREENS(mi), sizeof (providencestruct))) == NULL)
      return;
  }
  mp = &providence[MI_SCREEN(mi)];
  mp->trackball = gltrackball_init ();

  mono = MI_IS_MONO(mi);
  wire = MI_IS_WIREFRAME(mi);

  if((mp->glx_context = init_GL(mi)) != NULL) {
    reshape_providence(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    glDrawBuffer(GL_BACK);
    pinit();
  }
  else
    MI_CLEARWINDOW(mi);
}

void draw_providence(ModeInfo * mi) {
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
  
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();

  /* modify camera */
  if(fabs(camera_velocity) > EPSILON) {
    camera_z = max(min(camera_z + 0.1*camera_velocity, -4.0), -12.0);
    camera_velocity = 0.95*camera_velocity;
  }
  
  /* rotate providence */
  glTranslatef(0.0, 0.0, camera_z + sin(theta/4.0));
  glRotatef(10.0+20.0*sin(theta/2.0), 1.0, 0.0, 0.0);
  gltrackball_rotate(mp->trackball);
  glRotatef(theta * 180.0 / Pi, 0.0, -1.0, 0.0);

  /* draw providence */
  draw_providence_strip(mi);
  glPopMatrix();
  
  if(MI_IS_FPS(mi)) do_fps (mi);
  glFlush();
  
  glXSwapBuffers(display, window);

  /* update */
  currenttime += 1.0 / FPS;
  theta = currenttime / 2.0;
  update_particles();
}

void change_providence(ModeInfo * mi) {
  providencestruct *mp = &providence[MI_SCREEN(mi)];
  
  if (!mp->glx_context)
	return;
  
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(mp->glx_context));
  pinit();
}
