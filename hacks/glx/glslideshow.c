/*
 * glslideshow - takes a snapshot of the screen and smoothly scans around
 *               in it
 *
 * Copyright (c) 2002, 2003 Mike Oliphant (oliphant@gtk.org)
 *
 * Framework based on flipscreen3d
 *   Copyright (C) 2001 Ben Buxton (bb@cactii.net)
 *
 * Smooth transitions between multiple files added by
 * Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 */

#include <X11/Intrinsic.h>


# define PROGCLASS "GLSlideshow"
# define HACK_INIT init_slideshow
# define HACK_DRAW draw_slideshow
# define HACK_RESHAPE reshape_slideshow
# define slideshow_opts xlockmore_opts

# define DEF_FADE     "True"
# define DEF_DURATION "30"
# define DEF_ZOOM     "75"

#define DEFAULTS  "*delay:       20000          \n" \
                  "*fade:       " DEF_FADE     "\n" \
                  "*duration:   " DEF_DURATION "\n" \
                  "*zoom:       " DEF_ZOOM     "\n" \
		  "*wireframe:	 False          \n" \
                  "*showFPS:     False          \n" \
	          "*fpsSolid:    True           \n" \
                  "*desktopGrabber: xscreensaver-getimage -no-desktop %s\n"

# include "xlockmore.h"

#define RRAND(range) (random()%(range))
#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#ifdef USE_GL

#include <GL/glu.h>
#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include "grab-ximage.h"


#define QW 12.4   /* arbitrary size of the textured quads we render */
#define QH 12.4
#define QX -6.2
#define QY  6.2

#define NQUADS 2  /* sometimes we draw 2 at once */

typedef struct {
  GLXContext *glx_context;
  Window window;

  int tw, th;			/* texture width, height */
  GLfloat max_tx, max_ty;

  GLfloat qw, qh;		/* q? are for the quad we'll draw */
  GLfloat qx[NQUADS], qy[NQUADS], qz[NQUADS];
  GLfloat dx[NQUADS], dy[NQUADS], dz[NQUADS];

  GLuint texids[NQUADS];	/* two textures: current img, incoming img */

  time_t start_time;		/* when we started displaying this image */

  int curr_screen;
  int curr_texid_index;
  int in_transition;		/* true while we're drawing overlapping imgs */
  int in_file_transition;	/* ...plus loading a new image */
  int frames;			/* how many frames we've drawn in this pan */

} slideshow_state;

static slideshow_state *sss = NULL;


/* Command-line arguments
 */
int fade;     /* If true, transitions between pans (and between images) will
                 be translucent; otherwise, they will be jump-cuts. */
int duration; /* how many seconds until loading a new image */
int zoom;     /* how far in to zoom when panning, in percent of image size:
                 that is, 75 means "when zoomed all the way in, 75% of the
                 image will be on screen."  */


/* blah, apparently other magic numbers elsewhere in the file also
   affect this...   can't just change these to speed up / slow down...
 */
static int frames_per_pan  = 300;
static int frames_per_fade = 100;


static XrmOptionDescRec opts[] = {
  {"+fade",     ".slideshow.fade",     XrmoptionNoArg, (caddr_t) "False" },
  {"-fade",     ".slideshow.fade",     XrmoptionNoArg, (caddr_t) "True" },
  {"-duration", ".slideshow.duration", XrmoptionSepArg, 0},
  {"-zoom",     ".slideshow.zoom",     XrmoptionSepArg, 0}
};

static argtype vars[] = {
  {(caddr_t *) &fade,     "fade",     "Fade",     DEF_FADE,     t_Bool},
  {(caddr_t *) &duration, "duration", "Duration", DEF_DURATION, t_Int},
  {(caddr_t *) &zoom,     "zoom",     "Zoom",     DEF_ZOOM,     t_Int}
};

ModeSpecOpt slideshow_opts = {countof(opts), opts, countof(vars), vars, NULL};


/* draw the texture mapped quad.
   `screen' specifies which of the independently-moving images to draw.
 */
static void
showscreen (ModeInfo *mi, int wire, int screen, int texid_index)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  static GLfloat r = 1, g = 1, b = 1, a = 1;
  GLfloat qxw, qyh;
  GLfloat x, y, w, h;

  if (screen >= NQUADS) abort();
  qxw = ss->qx[screen] + ss->qw;
  qyh = ss->qy[screen] - ss->qh;
  x = ss->qx[screen];
  y = ss->qy[screen];
  w = qxw;
  h = qyh;

  ss->qx[screen] += ss->dx[screen];
  ss->qy[screen] -= ss->dy[screen];
  ss->qz[screen] += ss->dz[screen];

  glTranslatef(0, 0, ss->qz[screen]);

  if (ss->in_transition) {
    a = 1 - (ss->frames/100.0);

    if (screen != ss->curr_screen) {
      a = 1-a;
    }
  }
  else {
    a = 1;
  }

  glColor4f(r, g, b, a);

  if(!wire) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindTexture (GL_TEXTURE_2D, ss->texids[texid_index]);
  }

  glBegin(wire ? GL_LINE_LOOP : GL_QUADS);

  glNormal3f(0, 0, 1);

  glTexCoord2f(0, ss->max_ty);
  glVertex3f(x, y, 0);

  glTexCoord2f(ss->max_tx, ss->max_ty);
  glVertex3f(w, y, 0);

  glTexCoord2f(ss->max_tx, 0);
  glVertex3f(w, h, 0);

  glTexCoord2f(0, 0);
  glVertex3f(x, h, 0);

  glEnd();

  if (wire) {
    GLfloat i;
    int k = 10;
    glBegin(GL_LINES);
    for (i = x; i < w; i += ((w-x)/k))
      {
        glVertex3f(i, y, 0);
        glVertex3f(i, h, 0);
      }
    for (i = y; i >= h; i -= ((y-h)/k))
      {
        glVertex3f(x, i, 0);
        glVertex3f(w, i, 0);
      }
    glVertex3f(x, y, 0);
    glVertex3f(w, h, 0);
    glVertex3f(x, h, 0);
    glVertex3f(w, y, 0);
    glEnd();
  }

  glDisable(GL_TEXTURE_2D);
  glDepthMask(GL_TRUE);
  
  glBegin(GL_LINE_LOOP);
  glVertex3f(x, y, 0);
  glVertex3f(x, h, 0);
  glVertex3f(w, h, 0);
  glVertex3f(w, y, 0);
  glEnd();
  glDisable(GL_BLEND);

  glTranslatef(0, 0, -ss->qz[screen]);
}


static void
display (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  gluLookAt(0, 0, 15,
            0, 0, 0,
            0, 1, 0);
  glPushMatrix();

  showscreen (mi, wire, ss->curr_screen, ss->curr_texid_index);

  if (ss->in_transition)
    showscreen (mi, wire, 1-ss->curr_screen,
                (ss->in_file_transition
                 ? 1 - ss->curr_texid_index
                 : ss->curr_texid_index));

  glPopMatrix();
  glFlush();
}

void
reshape_slideshow (ModeInfo *mi, int width, int height)
{
  glViewport(0,0,(GLint)width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45, 1, 2.0, 85);
  glMatrixMode(GL_MODELVIEW);
}

static void
reset (ModeInfo *mi, int screen)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  ss->frames = 0;

  if (screen >= NQUADS) abort();
  ss->dz[screen] = (-.02+(RRAND(400)/10000.0)) * (GLfloat) zoom/100.0;

  if (ss->dz[screen] < 0.0) {
    ss->qz[screen] = 6.0 + RRAND(300)/100.0;
  }
  else {
    ss->qz[screen] = 1.0 + RRAND(300)/100.0;
  }

  ss->qz[screen] *= (GLfloat) zoom/100.0;

  ss->dx[screen] = -.02 + RRAND(400)/10000.0;
  ss->dy[screen] =- .01 + RRAND(200)/10000.0;

  ss->dx[screen] *= ss->qz[screen]/12.0;
  ss->dy[screen] *= ss->qz[screen]/12.0;

  ss->qx[screen] = QX - ss->dx[screen] * 40.0 * ss->qz[screen];
  ss->qy[screen] = QY + ss->dy[screen] * 40.0 * ss->qz[screen];  
}


static void
getSnapshot (ModeInfo *mi, int into_texid)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  XImage *ximage;
  int status;
  
  if(MI_IS_WIREFRAME(mi)) return;

  ss->qw = QW;
  ss->qh = QH;

  ximage = screen_to_ximage (mi->xgwa.screen, mi->window);

  ss->tw = mi->xgwa.width;
  ss->th = mi->xgwa.height;
/*  ss->tw = ximage->width; */
/*  ss->th = ximage->height; */
  
  ss->qw *= (GLfloat) ss->tw / MI_WIDTH(mi);
  ss->qh *= (GLfloat) ss->th / MI_HEIGHT(mi);
  
  ss->max_tx = (GLfloat) ss->tw / (GLfloat) ximage->width;
  ss->max_ty = (GLfloat) ss->th / (GLfloat) ximage->height;

  glBindTexture (GL_TEXTURE_2D, into_texid);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  
  clear_gl_error();
  status = gluBuild2DMipmaps(GL_TEXTURE_2D, 3,
			     ximage->width, ximage->height,
			     GL_RGBA, GL_UNSIGNED_BYTE, ximage->data);
  
  if(!status && glGetError())
   /* Some implementations of gluBuild2DMipmaps(), but set a GL error anyway.
      We could just call check_gl_error(), but that would exit. */
    status = -1;

  if(status) {
    const char *s = gluErrorString (status);

    fprintf(stderr, "%s: error mipmapping %dx%d texture: %s\n",
	    progname, ximage->width, ximage->height,
	    (s ? s : "(unknown)"));
    fprintf(stderr, "%s: turning on -wireframe.\n", progname);
    MI_IS_WIREFRAME(mi) = 1;
    clear_gl_error();
  }

  check_gl_error("mipmapping");  /* should get a return code instead of a
				    GL error, but just in case... */
  
  free(ximage->data);
  ximage->data = 0;
  XDestroyImage(ximage);

  ss->start_time = time ((time_t *) 0);
}

void
init_slideshow (ModeInfo *mi)
{
  int screen = MI_SCREEN(mi);
  slideshow_state *ss;
  
  if(sss == NULL) {
    if((sss = (slideshow_state *)
        calloc(MI_NUM_SCREENS(mi), sizeof(slideshow_state))) == NULL)
      return;
  }

  ss = &sss[screen];
  ss->window = MI_WINDOW(mi);
  ss->qw = QW;
  ss->qh = QH;

  if((ss->glx_context = init_GL(mi)) != NULL) {
    reshape_slideshow(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
  } else {
    MI_CLEARWINDOW(mi);
  }

  glClearColor(0.0,0.0,0.0,0.0);
  
  if(! MI_IS_WIREFRAME(mi)) {
    glShadeModel(GL_SMOOTH);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glDisable(GL_LIGHTING);

    glGenTextures (1, &ss->texids[0]);  /* texture for image A */
    glGenTextures (1, &ss->texids[1]);  /* texture for image B */
  }
  
  reset(mi, ss->curr_screen);
  ss->curr_texid_index = 0;
  getSnapshot(mi, ss->texids[ss->curr_texid_index]);
}

void
draw_slideshow (ModeInfo *mi)
{
  slideshow_state *ss = &sss[MI_SCREEN(mi)];
  Window w = MI_WINDOW(mi);
  Display *disp = MI_DISPLAY(mi);

  if(!ss->glx_context) return;

  glXMakeCurrent(disp, w, *(ss->glx_context));
  
  if (ss->frames == frames_per_pan) {

    time_t now = time ((time_t *) 0);

    if(fade) {
      ss->in_transition = 1;
      reset (mi, 1 - ss->curr_screen);

      if (ss->start_time + duration <= now) {
        ss->in_file_transition = 1;
        getSnapshot(mi, ss->texids[1 - ss->curr_texid_index]);
      }

    } else {
      reset(mi, ss->curr_screen);

      if (ss->start_time + duration <= now)
        getSnapshot(mi, ss->texids[ss->curr_texid_index]);
    }
  }

  if (fade && ss->in_transition && ss->frames == frames_per_fade) {
    ss->in_transition = 0;
    ss->curr_screen = 1 - ss->curr_screen;

    if (ss->in_file_transition) {
      ss->in_file_transition = 0;
      ss->curr_texid_index = 1 - ss->curr_texid_index;
    }
  }

  display(mi);
  
  ss->frames++;

  if(mi->fps_p) do_fps(mi);

  glFinish(); 
  glXSwapBuffers(disp, w);
}

void
release_slideshow (ModeInfo *mi)
{
  if(sss != NULL) {
    (void) free((void *) sss);
    sss = NULL;
  }

  FreeAllGL(MI);
}

#endif
