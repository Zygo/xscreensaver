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

#include <X11/Intrinsic.h>

#ifdef STANDALONE
#define PROGCLASS	    "AntSpotlight"
#define HACK_INIT	    init_antspotlight
#define HACK_DRAW	    draw_antspotlight
#define HACK_RESHAPE	    reshape_antspotlight
#define HACK_HANDLE_EVENT   antspotlight_handle_event
#define EVENT_MASK	    PointerMotionMask
#define antspotlight_opts	    xlockmore_opts
#define DEFAULTS	    "*delay:   20000   \n" \
			    "*showFPS: False   \n"

#include "xlockmore.h"
#else
#include "xlock.h"
#endif

#include <GL/glu.h>
#include "rotator.h"
#include "gltrackball.h"

ModeSpecOpt antspotlight_opts = {
  0, NULL, 0, NULL, NULL
};

#ifdef USE_MODULES
ModStruct   antspotlight_description = {
  "antspotlight", "init_antspotlight", "draw_antspotlight", 
  "release_antspotlight", "draw_antspotlight", "change_antspotlight", 
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

int winw, winh;
int tw, th; /* texture width, height */
int tx, ty;
GLfloat max_tx, max_ty;
#define QW 12
#define QH 12
GLfloat qw = QW, qh = QH; /* for the quad we'll draw */
GLfloat qx = -6 , qy = 6;
int mono = 0, wire = 0, ticks = 0;

typedef struct {
  GLint       WindH, WindW;
  GLXContext *glx_context;
  rotator    *rot;
  trackball_state *trackball;
  Bool        button_down_p;
} antspotlightstruct;

#include "ants.h"
#include "grab-ximage.h"

static antspotlightstruct *antspotlight = (antspotlightstruct *) NULL;

#define NUM_SCENES      2

/* draw method for ant */
Bool draw_ant(GLfloat *Material, int mono, int shadow, 
	      float ant_step, Bool (*sphere)(float), Bool (*cone)(float)) {
  
  float cos1 = cos(ant_step);
  float cos2 = cos(ant_step + 2 * Pi / 3);
  float cos3 = cos(ant_step + 4 * Pi / 3);
  float sin1 = sin(ant_step);
  float sin2 = sin(ant_step + 2 * Pi / 3);
  float sin3 = sin(ant_step + 4 * Pi / 3);
  
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mono ? MaterialGray5 : Material);
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
    if(mono)
      glColor3fv(MaterialGray8);
    else
      glColor3fv(MaterialMagenta);
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

/* no cone */
static Bool myCone2(float radius) { return True; }

Ant *ant;

double boardsize = 8.0;
#define BOARDLIST 1

GLfloat spot_direction[3];

void draw_board(void) {
  int i, j;
  double cutoff = Pi/3.0;
  double center[3];
  double centertex[2];

  glEnable(GL_TEXTURE_2D);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray6);

  /* draw mesh */

  /* center is roughly spotlight position */
  center[0] = ant->position[0];/* + cos(ant->direction); */
  center[1] = 0.0;
  center[2] = ant->position[2];/* - 0.7*sin(ant->direction);*/

  centertex[0] = (boardsize/2.0+center[0]) * max_tx / boardsize;
  centertex[1] = max_ty - ((boardsize/2.0+center[2]) * max_ty / boardsize);

/*   glPolygonMode(GL_FRONT, GL_LINE); */
/*   glDisable(GL_TEXTURE_2D); */

  /* 
     the vertices determined here should correspond to the illuminated
     board.  ideally the code adapts vertex distribution to the
     intensity and shape of the light.
     
     i should be finding the intersection of the cone of light and
     the board-plane.
  */
  for(i = -8; i < 8; ++i) {

    double theta1, theta2;

    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f(0.0, 1.0, 0.0);
    glTexCoord2f(centertex[0], centertex[1]);
    glVertex3f(center[0], 0.01, center[2]);

    /* watch those constants */
    theta1 = ant->direction + i*(cutoff/8);
    theta2 = ant->direction + (i+1)*(cutoff/8);

    for(j = 1; j <= 40; ++j) {
      double point[3], tex[2];
      /* double fj = pow(1.05, j) - 1.0;*/
      double fj = j / 6.0;
      point[0] = center[0] + fj*cos(theta1);
      point[1] = 0.0;
      point[2] = center[2] - fj*sin(theta1);

      tex[0] = (boardsize/2.0+point[0]) * max_tx / boardsize;
      tex[1] = max_ty - ((boardsize/2.0+point[2]) * max_ty / boardsize);

      glTexCoord2f(tex[0], tex[1]);
      glVertex3f(point[0], point[1], point[2]);

      point[0] = center[0] + fj*cos(theta2);
      point[1] = 0.0;
      point[2] = center[2] - fj*sin(theta2);

      tex[0] = (boardsize/2.0+point[0]) * max_tx / boardsize;
      tex[1] = max_ty - ((boardsize/2.0+point[2]) * max_ty / boardsize);

      glTexCoord2f(tex[0], tex[1]);
      glVertex3f(point[0], point[1], point[2]);
    }

    glEnd();
  }

  glDisable(GL_TEXTURE_2D);
}

/* return euclidean distance between two points */
double distance(double x[3], double y[3]) {
  double dx = x[0] - y[0];
  double dz = x[2] - y[2];
  return sqrt(dx*dx + dz*dz);
}

/* determine a new goal */
void find_goal(void) {
  do {
    ant->goal[0] = random()%((int)(boardsize+0.5)-2) - boardsize/2.0 + 1.0;
    ant->goal[1] = 0.0;
    ant->goal[2] = random()%((int)(boardsize+0.5)-2) - boardsize/2.0 + 1.0;
  }
  while(distance(ant->position, ant->goal) < 1.0);
}

/* construct our ant */
void build_ant(void) {
  ant = (Ant *) malloc(sizeof (Ant));
  ant->position[0] = 0.0;
  ant->position[1] = 0.0;
  ant->position[2] = 0.0;
  ant->direction = 0.0;
  ant->velocity = 0.02;
  ant->material = MaterialGray5;
  ant->step = 0;
  find_goal();
}

#define EPSILON 0.01

double sign(double d) {
  return d < 0.0 ? -1.0 : 1.0;
}

double min(double a, double b) {
  return a < b ? a : b;
}

double max(double a, double b) {
  return a > b ? a : b;
}

/* find a new goal and reset steps */
void reset_ant(void) {
  find_goal();
}

/* draw ant composed of skeleton and glass */
void show_ant(void) {

  glPushMatrix();

  /* move into position */
  glTranslatef(ant->position[0], 0.33, ant->position[2]);
  glRotatef(180.0 + ant->direction*180.0/Pi, 0.0, 1.0, 0.0);
  glRotatef(90.0, 0.0, 0.0, 1.0);

  /* draw skeleton */
  draw_ant(ant->material, mono, 0, ant->step, mySphere2, myCone2);

  /* draw glass */
  if(!wire && !mono) {
    glEnable(GL_BLEND);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGrayB);
    glColor4fv(MaterialGrayB);
    draw_ant(MaterialGrayB, mono, 0, ant->step, mySphere, myCone2);
    glDisable(GL_BLEND);
  }

  glPopMatrix();
}

void draw_antspotlight_strip(ModeInfo *mi) {

  /* compute spotlight position and direction */
  GLfloat light1_position[4];

  light1_position[0] = ant->position[0] + 0.7*cos(ant->direction);
  light1_position[1] = 0.5;
  light1_position[2] = ant->position[2] - 0.7*sin(ant->direction);
  light1_position[3] = 1.0;

  spot_direction[0] = cos(ant->direction);
  spot_direction[1] = -0.5;
  spot_direction[2] = -sin(ant->direction);

  glLightfv(GL_LIGHT2, GL_POSITION, light1_position);
  glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, spot_direction);
  
  glEnable(GL_LIGHT2);
  glDisable(GL_LIGHT0);
  glDisable(GL_LIGHT1);

  /* draw board */
  if(wire)
    ;
  else
    draw_board();
/*     glCallList(BOARDLIST); */

  glDisable(GL_LIGHT2);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  
  /* now modify ant */
  show_ant();

  /* near goal, bend path towards next step */
  if(distance(ant->position, ant->goal) < 0.2) {
    reset_ant();
  }

  /* move toward goal, correct ant direction if required */
  else {
    
    /* difference */
    double dx = ant->goal[0] - ant->position[0];
    double dz = -(ant->goal[2] - ant->position[2]);
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
    
    ideal = theta - ant->direction;
    if(ideal > Pi)
      ideal -= 2*Pi;
    
    /* compute correction */
    dt = sign(ideal) * min(fabs(ideal), Pi/100.0);
    ant->direction += dt;
    while(ant->direction < 0.0)
      ant->direction += 2*Pi;
    while(ant->direction > 2*Pi)
      ant->direction -= 2*Pi;
  }

  ant->position[0] += ant->velocity * cos(ant->direction);
  ant->position[2] += ant->velocity * sin(-ant->direction);
  ant->step += 10*ant->velocity;
  while(ant->step > 2*Pi)
    ant->step -= 2*Pi;
}

void reshape_antspotlight(ModeInfo * mi, int width, int height) {
  double h = (GLfloat) height / (GLfloat) width;  
  int size = 2;
  antspotlightstruct *mp = &antspotlight[MI_SCREEN(mi)];

  glViewport(0, 0, mp->WindW = (GLint) width, mp->WindH = (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective(45, 1/h, 1.0, 25.0);

  glMatrixMode(GL_MODELVIEW);
  glLineWidth(size);
  glPointSize(size);
}

/* lighting variables */
GLfloat front_shininess[] = {60.0};
GLfloat front_specular[] = {0.8, 0.8, 0.8, 1.0};
GLfloat ambient[] = {0.4, 0.4, 0.4, 1.0};
GLfloat ambient2[] = {0.0, 0.0, 0.0, 0.0};
GLfloat diffuse[] = {1.0, 1.0, 1.0, 1.0};
GLfloat position0[] = {1.0, 5.0, 1.0, 0.0};
GLfloat position1[] = {-1.0, -5.0, 1.0, 0.0};
GLfloat lmodel_ambient[] = {0.8, 0.8, 0.8, 1.0};
GLfloat lmodel_twoside[] = {GL_TRUE};
GLfloat spotlight_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
GLfloat spotlight_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };

static void pinit(void) {
  glClearDepth(1.0);
  glClearColor(0.0, 0.0, 0.0, 1.0);
  
  /* setup twoside lighting */
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, position0);
  glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT1, GL_POSITION, position1);
/*   glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient); */
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

/* cleanup routine */
void release_antspotlight(ModeInfo * mi) {

  if(antspotlight) {
    free((void *) antspotlight);
    antspotlight = (antspotlightstruct *) NULL;
  }

  FreeAllGL(mi);
}

int mag = 1;
#define MAX_MAGNIFICATION 10
#define max(a, b) a < b ? b : a
#define min(a, b) a < b ? a : b

/* event handling */
Bool antspotlight_handle_event(ModeInfo *mi, XEvent *event) {
  antspotlightstruct *mp = &antspotlight[MI_SCREEN(mi)];

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

/* get screenshot */
void get_snapshot(ModeInfo *modeinfo) {
  XImage *ximage;
  int status;

  if(MI_IS_WIREFRAME(modeinfo))
    return;

  ximage = screen_to_ximage(modeinfo->xgwa.screen, modeinfo->window, NULL);

  qw = QW; qh = QH;
  tw = modeinfo->xgwa.width;
  th = modeinfo->xgwa.height;
  
#if 0  /* jwz: this makes the image start off the bottom right of the screen */
  qx += (qw*tw/winw);
  qy -= (qh*th/winh);
#endif

  qw *= (GLfloat)tw/winw;
  qh *= (GLfloat)th/winh;

  max_tx = (GLfloat) tw / (GLfloat) ximage->width;
  max_ty = (GLfloat) th / (GLfloat) ximage->height;

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  
  clear_gl_error();
  status = gluBuild2DMipmaps(GL_TEXTURE_2D, 3,
			     ximage->width, ximage->height,
			     GL_RGBA, GL_UNSIGNED_BYTE, ximage->data);
  
  if (!status && glGetError())
    /* Some implementations of gluBuild2DMipmaps(), but set a GL error anyway.
       We could just call check_gl_error(), but that would exit. */
    status = -1;
  
  if(status) {
    const unsigned char *s = gluErrorString(status);
    fprintf(stderr, "%s: error mipmapping %dx%d texture: %s\n",
	    progname, ximage->width, ximage->height,
	    (s ? s : (unsigned char *) "(unknown)"));
    fprintf(stderr, "%s: turning on -wireframe.\n", progname);
    MI_IS_WIREFRAME(modeinfo) = 1;
    clear_gl_error();
  }
  check_gl_error("mipmapping");  /* should get a return code instead of a
				    GL error, but just in case... */
  
  free(ximage->data);
  ximage->data = 0;
  XDestroyImage (ximage);
}

void init_antspotlight(ModeInfo *mi) {
  double rot_speed = 0.3;

  antspotlightstruct *mp;
  
  if(!antspotlight) {
    if((antspotlight = (antspotlightstruct *) 
	calloc(MI_NUM_SCREENS(mi), sizeof (antspotlightstruct))) == NULL)
      return;
  }
  mp = &antspotlight[MI_SCREEN(mi)];
  mp->rot = make_rotator (rot_speed, rot_speed, rot_speed, 1, 0, True);
  mp->trackball = gltrackball_init ();

  if((mp->glx_context = init_GL(mi)) != NULL) {
    reshape_antspotlight(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    glDrawBuffer(GL_BACK);
    pinit();
  }
  else
    MI_CLEARWINDOW(mi);

  get_snapshot(mi);
  build_ant();
  mono = MI_IS_MONO(mi);
  wire = MI_IS_WIREFRAME(mi);

/*   glNewList(BOARDLIST, GL_COMPILE); */
/*   draw_board(); */
/*   glEndList(); */
}

void draw_antspotlight(ModeInfo * mi) {
  antspotlightstruct *mp;
  
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  
  if(!antspotlight)
	return;
  mp = &antspotlight[MI_SCREEN(mi)];
  
  MI_IS_DRAWN(mi) = True;
  
  if(!mp->glx_context)
	return;
  
  glXMakeCurrent(display, window, *(mp->glx_context));
  
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glPushMatrix();

  /* position camera */

  /* follow focused ant */
  glTranslatef(0.0, 0.0, -10.0 - mag);
  glRotatef(35.0, 1.0, 0.0, 0.0);
  gltrackball_rotate(mp->trackball);
  glTranslatef(-ant->position[0], ant->position[1], -ant->position[2]);

  /* stable position */
/*   glTranslatef(0.0, 0.0, -10.0 - mag); */
/*   gltrackball_rotate(mp->trackball); */
/*   glRotatef(40.0, 1.0, 0.0, 0.0); */
/*   glRotatef(20.0, 0.0, 1.0, 0.0); */

  draw_antspotlight_strip(mi);

  ++ticks;
  
  glPopMatrix();
  
  if (MI_IS_FPS(mi)) do_fps (mi);
  glFlush();
  
  glXSwapBuffers(display, window);
}

void change_antspotlight(ModeInfo * mi) {
  antspotlightstruct *mp = &antspotlight[MI_SCREEN(mi)];
  
  if (!mp->glx_context)
	return;
  
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(mp->glx_context));
  pinit();
}
