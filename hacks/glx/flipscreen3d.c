/*
 * flipscreen3d - takes snapshots of the screen and flips it around
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

#ifdef STANDALONE
#define DEFAULTS "*delay:     20000 \n" \
                 "*showFPS:   False \n" \
                 "*wireframe: False \n" \
                 "*useSHM:    True  \n"

# define refresh_screenflip 0
# include "xlockmore.h"                         /* from the xscreensaver distribution */
# include "gltrackball.h"
#else  /* !STANDALONE */
# include "xlock.h"                                     /* from the xlockmore distribution */
#endif /* !STANDALONE */

/* lifted from lament.c */
#define RAND(n) ((long) ((random() & 0x7fffffff) % ((long) (n))))
#define RANDSIGN() ((random() & 1) ? 1 : -1)


#ifdef USE_GL

/* Should be in <GL/glext.h> */
# ifndef  GL_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
# endif
# ifndef  GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
# endif

static int rotate;

#define QW 12
#define QH 12

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


static XrmOptionDescRec opts[] = {
  {"+rotate", ".screenflip.rotate", XrmoptionNoArg, "false" },
  {"-rotate", ".screenflip.rotate", XrmoptionNoArg, "true" },
};


static argtype vars[] = {
  {&rotate, "rotate", "Rotate", "True", t_Bool},
};



ENTRYPOINT ModeSpecOpt screenflip_opts = {countof(opts), opts, countof(vars), vars, NULL};


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

  int winw, winh;
  int tw, th; /* texture width, height */
  GLfloat min_tx, min_ty;
  GLfloat max_tx, max_ty;
  GLfloat qx, qy, qw, qh; /* the quad we'll draw */

  int regrab;
  int fadetime; /* fade before regrab */

  trackball_state *trackball;
  Bool button_down_p;

  GLfloat show_colors[4];
  GLfloat stretch_val_x, stretch_val_y;
  GLfloat stretch_val_dx, stretch_val_dy;

  GLfloat curx, cury, curz;

  GLfloat rx, ry, rz;
  GLfloat rot, drot, odrot, ddrot, orot;
  float theta, rho, dtheta, drho, gamma, dgamma;

  GLuint texid;
  Bool mipmap_p;
  Bool waiting_for_image_p;
  Bool first_image_p;

  GLfloat anisotropic;

} Screenflip;

static Screenflip *screenflip = NULL;

#include "grab-ximage.h"

static const GLfloat viewer[] = {0.0, 0.0, 15.0};


ENTRYPOINT Bool
screenflip_handle_event (ModeInfo *mi, XEvent *event)
{
  Screenflip *c = &screenflip[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      c->button_down_p = True;
      gltrackball_start (c->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      c->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (c->trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           c->button_down_p)
    {
      gltrackball_track (c->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


/* draw the texture mapped quad (actually two back to back)*/
static void showscreen(Screenflip *c, int frozen, int wire)
{
  GLfloat x, y, w, h;

  if (c->fadetime) {
/*    r -= 0.02; g -= 0.02; b -= 0.02; */
    c->show_colors[3] -= 0.02;
    if (c->show_colors[3] < 0) {
      c->regrab = 1;
      c->fadetime = 0;
    }
  } else if (c->show_colors[3] < 0) {
    c->show_colors[0] = c->show_colors[1] = 
      c->show_colors[2] = c->show_colors[3] = 1;
    c->stretch_val_x = c->stretch_val_y = 
      c->stretch_val_dx = c->stretch_val_dy = 0;
  }
  if (c->stretch_val_dx == 0 && !frozen && !(random() % 25))
    c->stretch_val_dx = (float)(random() % 100) / 5000;
  if (c->stretch_val_dy == 0 && !frozen && !(random() % 25))
    c->stretch_val_dy = (float)(random() % 100) / 5000;
    
  x = c->qx;
  y = c->qy;
  w = c->qx+c->qw;
  h = c->qy-c->qh;

  if (!frozen) {
     w *= sin (c->stretch_val_x) + 1;
     x *= sin (c->stretch_val_x) + 1;
     if (!c->button_down_p) {
     if (!c->fadetime) c->stretch_val_x += c->stretch_val_dx;
     if (c->stretch_val_x > 2*M_PI && !(random() % 5))
       c->stretch_val_dx = (float)(random() % 100) / 5000;
     else
       c->stretch_val_x -= 2*M_PI;
     }

     if (!c->button_down_p && !c->fadetime) c->stretch_val_y += c->stretch_val_dy;
     h *= sin (c->stretch_val_y) / 2 + 1;
     y *= sin (c->stretch_val_y) / 2 + 1;
     if (!c->button_down_p) {
     if (c->stretch_val_y > 2*M_PI && !(random() % 5))
       c->stretch_val_dy = (float)(random() % 100) / 5000;
     else
       c->stretch_val_y -= 2*M_PI;
     }
  }

  glColor4f(c->show_colors[0], c->show_colors[1], 
            c->show_colors[2], c->show_colors[3]);

  if (!wire)
    {
      glEnable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
    }

  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);

  glNormal3f(0, 0, 1);
  glTexCoord2f(c->max_tx, c->max_ty); glVertex3f(w, h, 0);
  glTexCoord2f(c->max_tx, c->min_ty); glVertex3f(w, y, 0);
  glTexCoord2f(c->min_tx, c->min_ty); glVertex3f(x, y, 0);
  glTexCoord2f(c->min_tx, c->max_ty); glVertex3f(x, h, 0);

  glNormal3f(0, 0, -1);
  glTexCoord2f(c->min_tx, c->min_ty); glVertex3f(x, y, -0.05);
  glTexCoord2f(c->max_tx, c->min_ty); glVertex3f(w, y, -0.05);
  glTexCoord2f(c->max_tx, c->max_ty); glVertex3f(w, h, -0.05);
  glTexCoord2f(c->min_tx, c->max_ty); glVertex3f(x, h, -0.05);
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

static int inposition(Screenflip *c)
{
  GLfloat wx;
  GLfloat wy;
  wx = 0 - (c->qw/2);
  wy = (c->qh/2);

  if (c->curx == 0) c->curx = c->qx;
  if (c->cury == 0) c->cury = c->qy;
  if (c->regrab) {
     c->curz = 0;
     c->curx = c->qx;
     c->cury = c->qy;
     c->regrab = 0;
  }
  if (c->curz > -10 || c->curx > wx + 0.1 || c->curx < wx - 0.1 ||
         c->cury > wy + 0.1 || c->cury < wy - 0.1) {
    if (c->curz > -10)
      c->curz -= 0.05;
    if (c->curx > wx) {
       c->qx -= 0.02;
       c->curx -= 0.02;
    }
    if (c->curx < wx) {
       c->qx += 0.02;
       c->curx += 0.02;
    }
    if (c->cury > wy) {
       c->qy -= 0.02;
       c->cury -= 0.02;
    }
    if (c->cury < wy) {
       c->qy += 0.02;
       c->cury += 0.02;
    }
    glTranslatef(0, 0, c->curz);
    return 0;
  }
  glTranslatef(0, 0, c->curz);
  return 1;

}

#if 0
static void drawgrid(void)
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
#endif


static void display(Screenflip *c, int wire)
{
  int frozen;

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(viewer[0], viewer[1], viewer[2], 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
  glPushMatrix();

  if (inposition(c)) {
    frozen = 0;
    glTranslatef(5 * sin(c->theta), 5 * sin(c->rho), 10 * cos(c->gamma) - 10);
/* randomly change the speed */
    if (!c->button_down_p && !(random() % 300)) {
      if (random() % 2)
        c->drho = 1/60 - (float)(random() % 100)/3000;
      if (random() % 2)
        c->dtheta = 1/60 - (float)(random() % 100)/3000;
      if (random() % 2)
        c->dgamma = 1/60 - (float)(random() % 100)/3000;
    }
    gltrackball_rotate (c->trackball);
    if (rotate) glRotatef(c->rot, c->rx, c->ry, c->rz);
/* update variables with each frame */
    if(!c->button_down_p && !c->fadetime) {
      c->theta += c->dtheta;
      c->rho += c->drho;
      c->gamma += c->dgamma;
      c->rot += c->drot;
      c->drot += c->ddrot;
    }
/* dont let our rotation speed get too high */
    if (c->drot > 5 && c->ddrot > 0)
        c->ddrot = 0 - (GLfloat)(random() % 100) / 1000;
    else if (c->drot < -5 && c->ddrot < 0)
        c->ddrot = (GLfloat)(random() % 100) / 1000;
  } else { /* reset some paramaters */
    c->ddrot = 0.05 - (GLfloat)(random() % 100) / 1000;
    c->theta = c->rho = c->gamma = 0;
    c->rot = 0;
    frozen = 1;
  }
  if (!c->button_down_p && !c->fadetime && (c->rot >= 360 || c->rot <= -360) && !(random() % 7)) { /* rotate  change */
    c->rx = (GLfloat)(random() % 100) / 100;
    c->ry = (GLfloat)(random() % 100) / 100;
    c->rz = (GLfloat)(random() % 100) / 100;
  }
  if (c->odrot * c->drot < 0 && c->tw < c->winw && !(random() % 10)) {
    c->fadetime = 1;                /* randomly fade and get new snapshot */
  }
  c->orot = c->rot;
  c->odrot = c->drot;
  if (c->rot > 360 || c->rot < -360) /* dont overflow rotation! */
    c->rot -= c->rot;
  showscreen(c, frozen, wire);
  glPopMatrix();
  glFlush();
}

ENTRYPOINT void reshape_screenflip(ModeInfo *mi, int width, int height)
{
 Screenflip *c = &screenflip[MI_SCREEN(mi)];
 glViewport(0,0,(GLint)width, (GLint) height);
 glMatrixMode(GL_PROJECTION);
 glLoadIdentity();
 gluPerspective(45, 1, 2.0, 85);
 glMatrixMode(GL_MODELVIEW);
 c->winw = width;
 c->winh = height;
}

static void
image_loaded_cb (const char *filename, XRectangle *geometry,
                 int image_width, int image_height, 
                 int texture_width, int texture_height,
                 void *closure)
{
  Screenflip *c = (Screenflip *) closure;

  c->tw = texture_width;
  c->th = texture_height;
  c->min_tx = (GLfloat) geometry->x / c->tw;
  c->min_ty = (GLfloat) geometry->y / c->th;
  c->max_tx = (GLfloat) (geometry->x + geometry->width)  / c->tw;
  c->max_ty = (GLfloat) (geometry->y + geometry->height) / c->th;

  c->qx = -QW/2 + ((GLfloat) geometry->x * QW / image_width);
  c->qy =  QH/2 - ((GLfloat) geometry->y * QH / image_height);
  c->qw =  QW   * ((GLfloat) geometry->width  / image_width);
  c->qh =  QH   * ((GLfloat) geometry->height / image_height);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   (c->mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));

  if (c->anisotropic >= 1.0)
    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 
                     c->anisotropic);

  c->waiting_for_image_p = False;
  c->first_image_p = False;
}


static void getSnapshot (ModeInfo *modeinfo)
{
  Screenflip *c = &screenflip[MI_SCREEN(modeinfo)];

  if (MI_IS_WIREFRAME(modeinfo))
    return;

  c->waiting_for_image_p = True;
  c->mipmap_p = True;
  load_texture_async (modeinfo->xgwa.screen, modeinfo->window,
                      *c->glx_context, 0, 0, c->mipmap_p, c->texid,
                      image_loaded_cb, c);
}

ENTRYPOINT void init_screenflip(ModeInfo *mi)
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

 c->trackball = gltrackball_init ();

 if ((c->glx_context = init_GL(mi)) != NULL) {
      reshape_screenflip(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
 } else {
     MI_CLEARWINDOW(mi);
 }
 c->winh = MI_WIN_HEIGHT(mi);
 c->winw = MI_WIN_WIDTH(mi);
 c->qw = QW;
 c->qh = QH;
 c->qx = -6;
 c->qy = 6;

 c->rx = c->ry = 1;
 c->odrot = 1;

 c->show_colors[0] = c->show_colors[1] = 
   c->show_colors[2] = c->show_colors[3] = 1;

 glClearColor(0.0,0.0,0.0,0.0);

 if (! MI_IS_WIREFRAME(mi))
   {
     glShadeModel(GL_SMOOTH);
     glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
     glEnable(GL_DEPTH_TEST);
     glEnable(GL_CULL_FACE);
     glCullFace(GL_BACK);
     glDisable(GL_LIGHTING);
   }

 if (strstr ((char *) glGetString(GL_EXTENSIONS),
             "GL_EXT_texture_filter_anisotropic"))
   glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &c->anisotropic);
 else
   c->anisotropic = 0.0;

 glGenTextures(1, &c->texid);

 c->first_image_p = True;
 getSnapshot(mi);
}

ENTRYPOINT void draw_screenflip(ModeInfo *mi)
{
  Screenflip *c = &screenflip[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if (!c->glx_context)
      return;

  /* Wait for the first image; for subsequent images, load them in the
     background while animating. */
  if (c->waiting_for_image_p && c->first_image_p)
    return;

  glXMakeCurrent(disp, w, *(c->glx_context));

  glBindTexture(GL_TEXTURE_2D, c->texid);

  if (c->regrab)
    getSnapshot(mi);

  display(c, MI_IS_WIREFRAME(mi));

  if(mi->fps_p) do_fps(mi);
  glFinish(); 
  glXSwapBuffers(disp, w);
}

ENTRYPOINT void release_screenflip(ModeInfo *mi)
{
  if (screenflip != NULL) {
   (void) free((void *) screenflip);
   screenflip = NULL;
  }
  FreeAllGL(mi);
}

XSCREENSAVER_MODULE_2 ("FlipScreen3D", flipscreen3d, screenflip)

#endif
