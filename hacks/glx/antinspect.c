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

#ifdef STANDALONE
#define DEFAULTS	    "*delay:   20000   \n" \
			    "*showFPS: False   \n"

# define refresh_antinspect 0
# define release_antinspect 0
#include "xlockmore.h"
#else
#include "xlock.h"
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

#include "sphere.h"
#include "gltrackball.h"

#define DEF_SHADOWS  "True"

static int shadows;

static XrmOptionDescRec opts[] = {
  {"-shadows", ".antinspect.shadows", XrmoptionNoArg, "on"},
  {"+shadows", ".antinspect.shadows", XrmoptionNoArg, "off"}
};

static argtype vars[] = {
  {&shadows, "shadows", "Shadows", DEF_SHADOWS, t_Bool}
};

static OptionStruct desc[] = {
  {"-/+shadows", "turn on/off ant shadows"}
};

ENTRYPOINT ModeSpecOpt antinspect_opts = {sizeof opts / sizeof opts[0], 
			    opts, 
			    sizeof vars / sizeof vars[0], 
			    vars, 
			    desc};

#ifdef USE_MODULES
ModStruct   antinspect_description =
  {"antinspect", "init_antinspect", "draw_antinspect", (char *) NULL,
   "draw_antinspect", "change_antinspect", (char *) NULL, &antinspect_opts,
   1000, 1, 1, 1, 4, 1.0, "",
   "draws some ants", 0, NULL};
#endif

#define Scale4Window               0.3
#define Scale4Iconic               0.4

#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi                         M_PI
#endif

#define ObjAntinspectStrip 0
#define ObjAntBody      1
#define MaxObj          2

/*************************************************************************/

typedef struct {
  GLint       WindH, WindW;
  GLfloat     step;
  GLfloat     ant_position;
  GLXContext *glx_context;
  trackball_state *trackball;
  Bool        button_down_p;
  int linewidth;
  float ant_step;

} antinspectstruct;

static const float front_shininess[] = {60.0};
static const float front_specular[] =  {0.7, 0.7, 0.7, 1.0};
static const float ambient[] = {0.0, 0.0, 0.0, 1.0};
static const float diffuse[] = {1.0, 1.0, 1.0, 1.0};
static float position0[] = {0.0, 3.0, 0.0, 1.0};
static const float position1[] = {-1.0, -3.0, 1.0, 0.0};
static const float lmodel_ambient[] = {0.5, 0.5, 0.5, 1.0};
static const float lmodel_twoside[] = {GL_TRUE};

static const float MaterialRed[] =     {0.6, 0.0, 0.0, 1.0};
static const float MaterialOrange[] =  {1.0, 0.69, 0.00, 1.0};
static const float MaterialGray[] =    {0.2, 0.2, 0.2, 1.0};
static const float MaterialBlack[] =   {0.1, 0.1, 0.1, 0.4};
static const float MaterialShadow[] =   {0.3, 0.3, 0.3, 0.3};
static const float MaterialGray5[] =   {0.5, 0.5, 0.5, 0.3};
static const float MaterialGray6[] =   {0.6, 0.6, 0.6, 1.0};

static antinspectstruct *antinspect = (antinspectstruct *) NULL;

#define NUM_SCENES      2

enum {X, Y, Z, W};
enum {A, B, C, D};

/* create a matrix that will project the desired shadow */
static void shadowmatrix(GLfloat shadowMat[4][4],
                         const GLfloat groundplane[4],
                         const GLfloat lightpos[4]) 
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

static const GLfloat ground[4] = {0.0, 1.0, 0.0, -0.00001};

/* simple filled sphere */
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

/* caged sphere */
static Bool mySphere2(float radius) 
{
#if 0
  GLUquadricObj *quadObj;

  if((quadObj = gluNewQuadric()) == 0)
    return False;
  gluQuadricDrawStyle(quadObj, (GLenum) GLU_LINE);/*GLU_SILHOUETTE);*/
  gluSphere(quadObj, radius, 16, 8);
  gluDeleteQuadric(quadObj);
#else
    glPushMatrix();
    glScalef (radius, radius, radius);
    glRotatef (90, 1, 0, 0);
    unit_sphere (16, 8, True);
    glPopMatrix();
#endif

  return True;
}

/* null cone */
static Bool myCone2(float radius) 
{ 
  return True; 
}

/* draw an ant */
static Bool draw_antinspect_ant(ModeInfo *mi, antinspectstruct * mp, 
                                const float *Material, int mono,
                                Bool (*sphere)(float), Bool (*cone)(float)) 
{
  float       cos1 = cos(mp->ant_step);
  float       cos2 = cos(mp->ant_step + 2 * Pi / 3);
  float       cos3 = cos(mp->ant_step + 4 * Pi / 3);
  float       sin1 = sin(mp->ant_step);
  float       sin2 = sin(mp->ant_step + 2 * Pi / 3);
  float       sin3 = sin(mp->ant_step + 4 * Pi / 3);
  
  if (mono)
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray5);
  else
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, Material);
  glEnable(GL_CULL_FACE);
  glPushMatrix();
  glScalef(1, 1.3, 1);
  if (!((*sphere)(0.18)))
    return False;
  glScalef(1, 1 / 1.3, 1);
  glTranslatef(0.00, 0.30, 0.00);
  if (!((*sphere)(0.2)))
    return False;
  
  glTranslatef(-0.05, 0.17, 0.05);
  glRotatef(-90, 1, 0, 0);
  glRotatef(-25, 0, 1, 0);
  if (!((*cone)(0.05)))
    return False;
  glTranslatef(0.00, 0.10, 0.00);
  if (!((*cone)(0.05)))
    return False;
  glRotatef(25, 0, 1, 0);
  glRotatef(90, 1, 0, 0);
  
  glScalef(1, 1.3, 1);
  glTranslatef(0.15, -0.65, 0.05);
  if (!((*sphere)(0.25)))
    return False;
  glScalef(1, 1 / 1.3, 1);
  glPopMatrix();
  glDisable(GL_CULL_FACE);
  
  glDisable(GL_LIGHTING);
  
  /* ANTENNAS */
  glBegin(GL_LINES);
  if (mono)
    glColor3fv(MaterialGray5);
  else
    glColor3fv(Material);
  glVertex3f(0.00, 0.30, 0.00);
  glColor3fv(MaterialGray);
  glVertex3f(0.40, 0.70, 0.40);
  mi->polygon_count++;
  if (mono)
    glColor3fv(MaterialGray5);
  else
    glColor3fv(Material);
  glVertex3f(0.00, 0.30, 0.00);
  glColor3fv(MaterialGray);
  glVertex3f(0.40, 0.70, -0.40);
  mi->polygon_count++;
  glEnd();
  glBegin(GL_POINTS);
  if (mono)
    glColor3fv(MaterialGray6);
  else
    glColor3fv(Material);
  glVertex3f(0.40, 0.70, 0.40);
  mi->polygon_count++;
  glVertex3f(0.40, 0.70, -0.40);
  mi->polygon_count++;
  glEnd();
  
  /* LEFT-FRONT ARM */
  glBegin(GL_LINE_STRIP);
  if (mono)
    glColor3fv(MaterialGray5);
  else
    glColor3fv(Material);
  glVertex3f(0.00, 0.05, 0.18);
  glVertex3f(0.35 + 0.05 * cos1, 0.15, 0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 + 0.05 * cos1, 0.25 + 0.1 * sin1, 0.45);
  mi->polygon_count++;
  glEnd();
  
  /* LEFT-CENTER ARM */
  glBegin(GL_LINE_STRIP);
  if (mono)
    glColor3fv(MaterialGray5);
  else
    glColor3fv(Material);
  glVertex3f(0.00, 0.00, 0.18);
  glVertex3f(0.35 + 0.05 * cos2, 0.00, 0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 + 0.05 * cos2, 0.00 + 0.1 * sin2, 0.45);
  mi->polygon_count++;
  glEnd();
  mi->polygon_count++;
  
  /* LEFT-BACK ARM */
  glBegin(GL_LINE_STRIP);
  if (mono)
    glColor3fv(MaterialGray5);
  else
    glColor3fv(Material);
  glVertex3f(0.00, -0.05, 0.18);
  glVertex3f(0.35 + 0.05 * cos3, -0.15, 0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 + 0.05 * cos3, -0.25 + 0.1 * sin3, 0.45);
  mi->polygon_count++;
  glEnd();
  
  /* RIGHT-FRONT ARM */
  glBegin(GL_LINE_STRIP);
  if (mono)
    glColor3fv(MaterialGray5);
  else
    glColor3fv(Material);
  glVertex3f(0.00, 0.05, -0.18);
  glVertex3f(0.35 - 0.05 * sin1, 0.15, -0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 - 0.05 * sin1, 0.25 + 0.1 * cos1, -0.45);
  mi->polygon_count++;
  glEnd();
  
  /* RIGHT-CENTER ARM */
  glBegin(GL_LINE_STRIP);
  if (mono)
    glColor3fv(MaterialGray5);
  else
    glColor3fv(Material);
  glVertex3f(0.00, 0.00, -0.18);
  glVertex3f(0.35 - 0.05 * sin2, 0.00, -0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 - 0.05 * sin2, 0.00 + 0.1 * cos2, -0.45);
  mi->polygon_count++;
  glEnd();
  
  /* RIGHT-BACK ARM */
  glBegin(GL_LINE_STRIP);
  if (mono)
    glColor3fv(MaterialGray5);
  else
    glColor3fv(Material);
  glVertex3f(0.00, -0.05, -0.18);
  glVertex3f(0.35 - 0.05 * sin3, -0.15, -0.25);
  mi->polygon_count++;
  glColor3fv(MaterialGray);
  glVertex3f(-0.20 - 0.05 * sin3, -0.25 + 0.1 * cos3, -0.45);
  mi->polygon_count++;
  glEnd();
    
  glEnable(GL_LIGHTING);
  
  return True;
}

/* only works with 3 right now */
#define ANTCOUNT 3

static const float MaterialBen[4] = {0.25, 0.30, 0.46, 1.0};

static const float* antmaterial[ANTCOUNT] = 
  {MaterialRed, MaterialBen, MaterialOrange};
static double antposition[ANTCOUNT] = {0.0, 120.0, 240.0};
static const double antvelocity[ANTCOUNT] = {0.3, 0.3, 0.3};
static const double antsphere[ANTCOUNT] = {1.2, 1.2, 1.2};

/* permutations */
static const double antorder[6][ANTCOUNT] = {{0, 1, 2},
				       {0, 2, 1},
				       {2, 0, 1},
				       {2, 1, 0},
				       {1, 2, 0},
				       {1, 0, 2}};

/* draw the scene */
static Bool draw_antinspect_strip(ModeInfo * mi) 
{
  antinspectstruct *mp = &antinspect[MI_SCREEN(mi)];
  int         i, j;
  int         mono = MI_IS_MONO(mi);

  int ro = (((int)antposition[1])/(360/(2*ANTCOUNT))) % (2*ANTCOUNT);

  glEnable(GL_TEXTURE_2D);
  position0[1] = 9.6;
  glLightfv(GL_LIGHT0, GL_POSITION, position0);

  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray5);
  glRotatef(-30.0, 0.0, 1.0, 0.0);

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);

  /* render ground plane */
  glBegin(GL_TRIANGLES);
  glColor4fv(MaterialShadow);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialBlack);
  glNormal3f(0.0, 1.0, 0.0);

  /* middle tri */
  glVertex3f(0.0, 0.0, -1.0);
  glVertex3f(-sqrt(3.0)/2.0, 0.0, 0.5);
  glVertex3f(sqrt(3.0)/2.0, 0.0, 0.5);
  mi->polygon_count++;
  glEnd();

  /* rotate */
  for(i = 0; i < 3; ++i) {
    glRotatef(120.0, 0.0, 1.0, 0.0);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0, 0.0, 1.0 + 3.0);
    glVertex3f(sqrt(3.0)/2.0, 0.0, -0.5 + 3.0);
    glVertex3f(-sqrt(3.0)/2.0, 0.0, -0.5 + 3.0);
    mi->polygon_count++;
    glEnd();
  }

  /* first render shadows -- no depth required */
  if(shadows) {
    GLfloat m[4][4];
    shadowmatrix(m, ground, position0);
    
    glColor4fv(MaterialShadow);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialShadow);
    
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    
    /* display ant shadow */
    glPushMatrix();
    glTranslatef(0.0, 0.001, 0.0);
    glMultMatrixf(m[0]);

    for(i = 0; i < ANTCOUNT; ++i) {

      /* draw ant */
      glPushMatrix();

      /* center */
      glRotatef(antposition[i], 0.0, 1.0, 0.0);
      glTranslatef(2.4, 0.0, 0.0);
      glTranslatef(0.0, antsphere[i], 0.0);
      glRotatef(90.0, 0.0, 1.0, 0.0);

      /* orient ant */
      glRotatef(10.0, 0.0, 1.0, 0.0);
      glRotatef(40.0, 0.0, 0.0, 1.0);
      glTranslatef(0.0, -0.8, 0.0);
      glRotatef(180.0, 0.0, 1.0, 0.0);
      glRotatef(90.0, 0.0, 0.0, 1.0);

      /* set colour */
      glColor4fv(MaterialShadow);
      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialShadow);

      if(antposition[i] > 360.0)
	antposition[i] = 0.0;
      draw_antinspect_ant(mi, mp, MaterialShadow, mono, mySphere2, myCone2);

      glDisable(GL_BLEND);
      glDisable(GL_LIGHTING);

      /* draw sphere */
      glRotatef(-20.0, 1.0, 0.0, 0.0);
      glRotatef(-mp->ant_step*2, 0.0, 0.0, 1.0);
      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialShadow);
      mySphere2(1.2);

      glPopMatrix();
    }
  
    glPopMatrix();
  }

  glEnable(GL_LIGHTING);

  /* truants */
  for(j = 0; j < ANTCOUNT; ++j) {
    /* determine rendering order */
    i = antorder[ro][j];

    glPushMatrix();
    
    /* center */
    glRotatef(antposition[i], 0.0, 1.0, 0.0);
    glTranslatef(2.4, 0.0, 0.0);
    glTranslatef(0.0, antsphere[i], 0.0);    
    glRotatef(90.0, 0.0, 1.0, 0.0);

    /* draw ant */
    glPushMatrix();
    glRotatef(10.0, 0.0, 1.0, 0.0);
    glRotatef(40.0, 0.0, 0.0, 1.0);
    glTranslatef(0.0, -0.8, 0.0);
    glRotatef(180.0, 0.0, 1.0, 0.0);
    glRotatef(90.0, 0.0, 0.0, 1.0);
    if(antposition[i] > 360.0)
      antposition[i] = 0.0;
    glEnable(GL_BLEND);
    draw_antinspect_ant(mi, mp, antmaterial[i], mono, mySphere2, myCone2);
    glDisable(GL_BLEND);
    glPopMatrix();

    /* draw sphere */
    glRotatef(-20.0, 1.0, 0.0, 0.0);
    glRotatef(-mp->ant_step*2, 0.0, 0.0, 1.0);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mono ? MaterialGray5 : antmaterial[i]);
    mySphere2(1.2);
    glEnable(GL_BLEND);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialBlack);
    mySphere(1.16);
    glDisable(GL_BLEND);
        
    glPopMatrix();

    /* finally, evolve */
    antposition[i] += antvelocity[i];
  }

  /* but the step size is the same! */
  mp->ant_step += 0.2;
  
  mp->ant_position += 1;
  return True;
}

ENTRYPOINT void reshape_antinspect(ModeInfo * mi, int width, int height) 
{
  double h = (GLfloat) height / (GLfloat) width;  
  antinspectstruct *mp = &antinspect[MI_SCREEN(mi)];
  mp->linewidth = (width / 512) + 1;

  glViewport(0, 0, mp->WindW = (GLint) width, mp->WindH = (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective(45, 1/h, 7.0, 20.0);

  glMatrixMode(GL_MODELVIEW);
  glLineWidth(mp->linewidth);
  glPointSize(mp->linewidth);
}

static void pinit(void) 
{
  glClearDepth(1.0);
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, position0);
  glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT1, GL_POSITION, position1);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
  glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  glEnable(GL_NORMALIZE);
  glFrontFace(GL_CCW);
  
  /* antinspect */
  glShadeModel(GL_SMOOTH);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);

  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);
}

ENTRYPOINT Bool antinspect_handle_event (ModeInfo *mi, XEvent *event) 
{
  antinspectstruct *mp = &antinspect[MI_SCREEN(mi)];
  
  if (gltrackball_event_handler (event, mp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &mp->button_down_p))
    return True;
  
  return False;
}

ENTRYPOINT void init_antinspect(ModeInfo * mi) 
{
  antinspectstruct *mp;
  
  MI_INIT(mi, antinspect, NULL);
  mp = &antinspect[MI_SCREEN(mi)];
  mp->step = NRAND(90);
  mp->ant_position = NRAND(90);
  mp->trackball = gltrackball_init (False);
  
  if ((mp->glx_context = init_GL(mi)) != NULL) {
    reshape_antinspect(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    glDrawBuffer(GL_BACK);
    pinit();
  } 
  else
    MI_CLEARWINDOW(mi);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

ENTRYPOINT void draw_antinspect(ModeInfo * mi) 
{
  antinspectstruct *mp;
  
  Display    *display = MI_DISPLAY(mi);
  Window      window = MI_WINDOW(mi);
  
  if(!antinspect)
    return;
  mp = &antinspect[MI_SCREEN(mi)];
  
  MI_IS_DRAWN(mi) = True;
  
  if(!mp->glx_context)
	return;
  
  glXMakeCurrent(display, window, *(mp->glx_context));
  
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();

  mi->polygon_count = 0;

  /* position camera --- this works well, we can peer inside 
     the antbubble */
  glTranslatef(0.0, 0.0, -10.0);

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
  {
    GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
    int o = (int) current_device_rotation();
    if (o != 0 && o != 180 && o != -180)
      glScalef (1/h, 1/h, 1/h);
    glRotatef(o, 0, 0, 1);
  }
# endif

  gltrackball_rotate(mp->trackball);
  glRotatef((15.0/2.0 + 15.0*sin(mp->ant_step/100.0)), 1.0, 0.0, 0.0);
  glRotatef(30.0, 1.0, 0.0, 0.0);
  glRotatef(180.0, 0.0, 1.0, 0.0);
  
  if (!draw_antinspect_strip(mi)) {
	MI_ABORT(mi);
	return;
  }
  
  glPopMatrix();
  
  if (MI_IS_FPS(mi)) do_fps (mi);
  glFlush();
  
  glXSwapBuffers(display, window);
  
  mp->step += 0.025;
}

#ifndef STANDALONE
ENTRYPOINT void change_antinspect(ModeInfo * mi) 
{
  antinspectstruct *mp = &antinspect[MI_SCREEN(mi)];
  
  if (!mp->glx_context)
	return;
  
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(mp->glx_context));
  pinit();
}
#endif /* !STANDALONE */


XSCREENSAVER_MODULE ("AntInspect", antinspect)
