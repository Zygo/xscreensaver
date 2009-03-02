/*
 * screenflip - takes snapshots of the screen and flips it around
 *
 * version 1.0 - Oct 24, 2001
 *
 * Copyright (C) 2001 Ben Buxton (bb@cactii.net)
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


#ifdef STANDALONE
# define PROGCLASS                                      "Screenflip"
# define HACK_INIT                                      init_screenflip
# define HACK_DRAW                                      draw_screenflip
# define HACK_RESHAPE                           reshape_screenflip
# define screenflip_opts                                     xlockmore_opts
/* insert defaults here */

#define DEFAULTS       "*delay:       20000       \n" \
                        "*showFPS:       False       \n" \
                        "*rotate:       True       \n" \
			"*wireframe:	False	\n"	\

# include "xlockmore.h"                         /* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"                                     /* from the xlockmore distribution */
#endif /* !STANDALONE */

/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)


#ifdef USE_GL

#include <GL/glu.h>

int rotate;

int winw, winh;
int tw, th; /* texture width, height */
int tx, ty;
GLfloat max_tx, max_ty;

#define QW 12
#define QH 12
GLfloat qw = QW, qh = QH; /* q? are for the quad we'll draw */
GLfloat qx = -6 , qy = 6;

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


static XrmOptionDescRec opts[] = {
  {"+rotate", ".screenflip.rotate", XrmoptionNoArg, (caddr_t) "false" },
  {"-rotate", ".screenflip.rotate", XrmoptionNoArg, (caddr_t) "true" },
};


static argtype vars[] = {
  {(caddr_t *) &rotate, "rotate", "Rotate", "True", t_Bool},
};



ModeSpecOpt screenflip_opts = {countof(opts), opts, countof(vars), vars, NULL};


#ifdef USE_MODULES
ModStruct   screenflip_description =
{"screenflip", "init_screenflip", "draw_screenflip", "release_screenflip",
 "draw_screenflip", "init_screenflip", NULL, &screenflip_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Screenflips", 0, NULL};

#endif


typedef struct {
  GLXContext *glx_context;
  Window window;
} Screenflip;

static Screenflip *screenflip = NULL;

#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include "grab-ximage.h"

#ifndef M_PI
#define M_PI 3.14159265
#endif

static GLfloat viewer[] = {0.0, 0.0, 15.0};

int regrab = 0;
int fadetime = 0; /* fade before regrab */


/* draw the texture mapped quad (actually two back to back)*/
void showscreen(int frozen, int wire)
{
  static GLfloat r = 1, g = 1, b = 1, a = 1;
  GLfloat qxw, qyh;
  GLfloat x, y, w, h;
  /* static int stretch; */
  static GLfloat stretch_val_x = 0, stretch_val_y = 0;
  static GLfloat stretch_val_dx = 0, stretch_val_dy = 0;
  /* static int stretch_x = 0, stretch_y = 0; */

  if (fadetime) {
/*    r -= 0.02; g -= 0.02; b -= 0.02; */
    a -= 0.02;
    if (a < 0) {
      regrab = 1;
      fadetime = 0;
    }
  } else if (a < 0) {
    r = g = b = a = 1;
    stretch_val_x = stretch_val_y = stretch_val_dx = stretch_val_dy = 0;
  }
  if (stretch_val_dx == 0 && !frozen && !(random() % 25))
    stretch_val_dx = (float)(random() % 100) / 5000;
  if (stretch_val_dy == 0 && !frozen && !(random() % 25))
    stretch_val_dy = (float)(random() % 100) / 5000;
    
  qxw = qx+qw;
  qyh = qy-qh;
  x = qx; y = qy;
  w = qxw; h = qyh;

  if (!frozen) {
     w *= sin (stretch_val_x) + 1;
     x *= sin (stretch_val_x) + 1;
     if (!fadetime) stretch_val_x += stretch_val_dx;
     if (stretch_val_x > 2*M_PI && !(random() % 5))
       stretch_val_dx = (float)(random() % 100) / 5000;
     else
       stretch_val_x -= 2*M_PI;

     if (!fadetime) stretch_val_y += stretch_val_dy;
     h *= sin (stretch_val_y) / 2 + 1;
     y *= sin (stretch_val_y) / 2 + 1;
     if (stretch_val_y > 2*M_PI && !(random() % 5))
       stretch_val_dy = (float)(random() % 100) / 5000;
     else
       stretch_val_y -= 2*M_PI;
  }

  glColor4f(r, g, b, a);

  if (!wire)
    {
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
    }

  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);

  glNormal3f(0, 0, 1);

  glTexCoord2f(0, max_ty);
  glVertex3f(x, y, 0);

  glTexCoord2f(max_tx, max_ty);
  glVertex3f(w, y, 0);

  glTexCoord2f(max_tx, 0);
  glVertex3f(w, h, 0);

  glTexCoord2f(0, 0);
  glVertex3f(x, h, 0);

  glNormal3f(0, 0, -1);

  glTexCoord2f(0, max_ty);
  glVertex3f(x, y, -0.05);

  glTexCoord2f(0, 0);
  glVertex3f(x, h, -0.05);

  glTexCoord2f(max_tx, 0);
  glVertex3f(w, h, -0.05);

  glTexCoord2f(max_tx, max_ty);
  glVertex3f(w, y, -0.05);

  glEnd();


  glDisable(GL_TEXTURE_2D);
  glDepthMask(GL_TRUE);

  glBegin(GL_LINE_LOOP);
   glVertex3f(x, y, 0);
   glVertex3f(x, h, 0);
   glVertex3f(w, h, 0);
   glVertex3f(w, y, 0);
 glEnd();
  glDisable(GL_BLEND);

}

/* This function is responsible for 'zooming back' the square after
 * a new chunk has been grabbed with getSnapshot(), and positioning
 * it suitably on the screen. Once positioned (where we begin to rotate),
 * it just does a glTranslatef() and returns 1
 */

int inposition(void)
{
  static GLfloat curx, cury, curz = 0;
  GLfloat wx;
  GLfloat wy;
  wx = 0 - (qw/2);
  wy = (qh/2);

  if (curx == 0) curx = qx;
  if (cury == 0) cury = qy;
  if (regrab) {
     curz = 0;
     curx = qx;
     cury = qy;
     regrab = 0;
  }
  if (curz > -10 || curx > wx + 0.1 || curx < wx - 0.1 ||
         cury > wy + 0.1 || cury < wy - 0.1) {
    if (curz > -10)
      curz -= 0.05;
    if (curx > wx) {
       qx -= 0.02;
       curx -= 0.02;
    }
    if (curx < wx) {
       qx += 0.02;
       curx += 0.02;
    }
    if (cury > wy) {
       qy -= 0.02;
       cury -= 0.02;
    }
    if (cury < wy) {
       qy += 0.02;
       cury += 0.02;
    }
    glTranslatef(0, 0, curz);
    return 0;
  }
  glTranslatef(0, 0, curz);
  return 1;

}

void drawgrid(void)
{
  int i;

  glColor3f(0, 0.7, 0);
  glBegin(GL_LINES);
  for (i = 0 ; i <= 50; i+=2) {
      glVertex3f( -25, -15, i-70);
      glVertex3f( 25, -15, i-70);
      glVertex3f( i-25, -15, -70);
      glVertex3f( i-25, -15, -20);
  }
  glEnd();
}

void display(int wire)
{
  static GLfloat rx=1, ry=1, rz=0;
  static GLfloat rot = 0;
  static GLfloat drot = 0;
  static GLfloat odrot = 1;
  static GLfloat ddrot = 0;
  static float theta = 0, rho = 0, dtheta = 0, drho = 0, gamma = 0, dgamma = 0;
  static GLfloat orot;
  int frozen;

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(viewer[0], viewer[1], viewer[2], 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
  glPushMatrix();

  if (inposition()) {
    frozen = 0;
    glTranslatef(5 * sin(theta), 5 * sin(rho), 10 * cos(gamma) - 10);
/* randomly change the speed */
    if (!(random() % 300)) {
      if (random() % 2)
        drho = 1/60 - (float)(random() % 100)/3000;
      if (random() % 2)
        dtheta = 1/60 - (float)(random() % 100)/3000;
      if (random() % 2)
        dgamma = 1/60 - (float)(random() % 100)/3000;
    }
    if (rotate) glRotatef(rot, rx, ry, rz);
/* update variables with each frame */
    if (!fadetime) {
      theta += dtheta;
      rho += drho;
      gamma += dgamma;
      rot += drot;
      drot += ddrot;
    }
/* dont let our rotation speed get too high */
    if (drot > 5 && ddrot > 0)
        ddrot = 0 - (GLfloat)(random() % 100) / 1000;
    else if (drot < -5 && ddrot < 0)
        ddrot = (GLfloat)(random() % 100) / 1000;
  } else { /* reset some paramaters */
    ddrot = 0.05 - (GLfloat)(random() % 100) / 1000;
    theta = rho = gamma = 0;
    rot = 0;
    frozen = 1;
  }
  if (!fadetime && (rot >= 360 || rot <= -360) && !(random() % 7)) { /* rotate  change */
    rx = (GLfloat)(random() % 100) / 100;
    ry = (GLfloat)(random() % 100) / 100;
    rz = (GLfloat)(random() % 100) / 100;
  }
  if (odrot * drot < 0 && tw < winw && !(random() % 10)) {
    fadetime = 1;                /* randomly fade and get new snapshot */
  }
  orot = rot;
  odrot = drot;
  if (rot > 360 || rot < -360) /* dont overflow rotation! */
    rot -= rot;
  showscreen(frozen, wire);
  glPopMatrix();
  glFlush();
}

void reshape_screenflip(ModeInfo *mi, int width, int height)
{
 glViewport(0,0,(GLint)width, (GLint) height);
 glMatrixMode(GL_PROJECTION);
 glLoadIdentity();
 gluPerspective(45, 1, 2.0, 85);
 glMatrixMode(GL_MODELVIEW);
 winw = width;
 winh = height;
}

void getSnapshot (ModeInfo *modeinfo)
{
  XImage *ximage;
  int status;

  if (MI_IS_WIREFRAME(modeinfo))
    return;

 ximage = screen_to_ximage (modeinfo->xgwa.screen, modeinfo->window);

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
 if (status)
   {
     const char *s = gluErrorString (status);
     fprintf (stderr, "%s: error mipmapping %dx%d texture: %s\n",
              progname, ximage->width, ximage->height,
              (s ? s : "(unknown)"));
     fprintf (stderr, "%s: turning on -wireframe.\n", progname);
     MI_IS_WIREFRAME(modeinfo) = 1;
     clear_gl_error();
   }
 check_gl_error("mipmapping");  /* should get a return code instead of a
                                   GL error, but just in case... */

 free(ximage->data);
 ximage->data = 0;
 XDestroyImage (ximage);
}

void init_screenflip(ModeInfo *mi)
{
  int screen = MI_SCREEN(mi);
  Screenflip *c;

 if (screenflip == NULL) {
   if ((screenflip = (Screenflip *) calloc(MI_NUM_SCREENS(mi),
                                        sizeof(Screenflip))) == NULL)
          return;
 }
 c = &screenflip[screen];
 c->window = MI_WINDOW(mi);

 if ((c->glx_context = init_GL(mi)) != NULL) {
      reshape_screenflip(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
 } else {
     MI_CLEARWINDOW(mi);
 }
 winh = MI_WIN_HEIGHT(mi);
 winw = MI_WIN_WIDTH(mi);
 glClearColor(0.0,0.0,0.0,0.0);

 if (! MI_IS_WIREFRAME(mi))
   {
     glShadeModel(GL_SMOOTH);
     glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
     glEnable(GL_DEPTH_TEST);
     glEnable(GL_CULL_FACE);
     glCullFace(GL_FRONT);
     glDisable(GL_LIGHTING);
   }

 getSnapshot(mi);
}

void draw_screenflip(ModeInfo *mi)
{
  Screenflip *c = &screenflip[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if (!c->glx_context)
      return;

 glXMakeCurrent(disp, w, *(c->glx_context));

  if (regrab)
    getSnapshot(mi);

  display(MI_IS_WIREFRAME(mi));

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

void release_screenflip(ModeInfo *mi)
{
  if (screenflip != NULL) {
   (void) free((void *) screenflip);
   screenflip = NULL;
  }
  FreeAllGL(MI);
}

#endif
