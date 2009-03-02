/* -*- Mode: C; tab-width: 4 -*- emacs friendly */
/* gflux - creates a fluctuating 3D grid 
 * requires OpenGL or MesaGL
 * 
 * Copyright (c) Josiah Pease, 2000, 2003
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Thanks go to all those who worked on...
 * MesaGL, OpenGL, UtahGLX, XFree86, gcc, vim, rxvt, the PNM (anymap) format
 * xscreensaver and the thousands of other tools, apps and daemons that make
 * linux usable  
 * Personal thanks to Kevin Moss, Paul Sheahee and Jamie Zawinski
 * 
 * some xscreensaver code lifted from superquadrics.  Most other glx hacks 
 * used as reference at some time.
 *
 * This hack and others can cause UtahGLX to crash my X server
 * wireframe looks good with software only rendering anyway
 * If anyone can work out why and supply a fix I'd love to hear from them
 * 
 * Josiah Pease <gfluxcode@jpease.force9.co.uk> 21 July 2000
 * 
 * History 
 * 10 June 2000 : wireframe rippling grid standalone written
 * 18 June 2000 : xscreensaver code added
 * 25 June 2000 : solid and light added
 * 04 July 2000 : majour bug hunt, xscreensaver code rewritten
 * 08 July 2000 : texture mapping, rotation and zoom added
 * 21 July 2000 : cleaned up code from bug hunts, manpage written
 * 24 November 2000 : fixed x co-ord calculation in solid - textured
 * 05 March 2001 : put back non pnmlib code with #ifdefs
 * 11 May 2002 : fixed image problems with large images
 */


#ifdef STANDALONE
# define PROGCLASS						"gflux"
# define HACK_INIT						init_gflux
# define HACK_DRAW						draw_gflux
# define HACK_RESHAPE					reshape_gflux
# define HACK_HANDLE_EVENT				gflux_handle_event
# define EVENT_MASK						PointerMotionMask
# define gflux_opts						xlockmore_opts
#define DEFAULTS                        "*delay:		20000   \n" \
										"*showFPS:      False   \n" \
                                        "*mode:         light" "\n" \
                                        "*squares:      19      \n" \
										"*resolution:   0       \n" \
                                        "*flat:         0       \n" \
                                        "*speed:        0.05    \n" \
                                        "*rotationx:    0.01    \n" \
                                        "*rotationy:    0.0     \n" \
                                        "*rotationz:    0.1     \n" \
                                        "*waves:        3       \n" \
                                        "*waveChange:   50      \n" \
                                        "*waveHeight:  0.8      \n" \
                                        "*waveFreq:    3.0      \n" \
                                        "*zoom:        1.0      \n" \
                                        "*useSHM:      True     \n" 


# include "xlockmore.h"				/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Drawing.h>
#else  /* VMS */
#  include <Xmu/Drawing.h>
# endif /* VMS */
#endif

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include <stdlib.h>
#include <stdio.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <math.h>

#include "grab-ximage.h"
#include "gltrackball.h"


static enum {wire=0,solid,light,checker,grab} _draw; /* draw style */
static int _squares = 19;                                 /* grid size */
static int _resolution = 4;                    /* wireframe resolution */
static int _flat = 0;

static float _speed = 0.05;
static float _rotationx = 0.01;
static float _rotationy = 0.0;
static float _rotationz = 0.1;
static float _zoom = 1.0;

static int _waves = 3;
static int _waveChange = 50;
static float _waveHeight = 1.0;
static float _waveFreq = 3.0;

static trackball_state *trackball;
static Bool button_down_p = False;

#define WIDTH 320
#define HEIGHT 240

static XrmOptionDescRec opts[] = {
    {"-squares", ".gflux.squares", XrmoptionSepArg, 0},
    {"-resolution", ".gflux.resolution", XrmoptionSepArg, 0},
/*    {"-draw", ".gflux.draw", XrmoptionSepArg, 0},*/
    {"-mode", ".gflux.mode", XrmoptionSepArg, 0},
    {"-flat", ".gflux.flat", XrmoptionSepArg, 0},
    {"-speed", ".gflux.speed", XrmoptionSepArg, 0},
    {"-rotationx", ".gflux.rotationx", XrmoptionSepArg, 0},
    {"-rotationy", ".gflux.rotationy", XrmoptionSepArg, 0},
    {"-rotationz", ".gflux.rotationz", XrmoptionSepArg, 0},
    {"-waves", ".gflux.waves", XrmoptionSepArg, 0},
    {"-waveChange", ".gflux.waveChange", XrmoptionSepArg, 0},
    {"-waveHeight", ".gflux.waveHeight", XrmoptionSepArg, 0},
    {"-waveFreq", ".gflux.waveFreq", XrmoptionSepArg, 0},
    {"-zoom", ".gflux.zoom", XrmoptionSepArg, 0},
};


static argtype vars[] = {
    {&_squares, "squares", "Squares", "19", t_Int},
    {&_resolution, "resolution", "Resolution", "4", t_Int},
/*    {&_draw, "draw", "Draw", "2", t_Int},*/
    {&_flat, "flat", "Flat", "0", t_Int},
    {&_speed, "speed", "Speed", "0.05", t_Float},
    {&_rotationx, "rotationx", "Rotationx", "0.01", t_Float},
    {&_rotationy, "rotationy", "Rotationy", "0.0", t_Float},
    {&_rotationz, "rotationz", "Rotationz", "0.1", t_Float},
    {&_waves, "waves", "Waves", "3", t_Int},
    {&_waveChange, "waveChange", "WaveChange", "50", t_Int},
    {&_waveHeight, "waveHeight", "WaveHeight", "1.0", t_Float},
    {&_waveFreq, "waveFreq", "WaveFreq", "3.0", t_Float},
    {&_zoom, "zoom", "Zoom", "1.0", t_Float},
};


static OptionStruct desc[] =
{
    {"-squares num", "size of grid in squares (19)"},
    {"-resolution num", "detail of lines making grid, wireframe only (4)"},
/*    {"-draw num", "draw method to use: 0=wireframe 1=solid 2=lit (0)"},*/
    {"-flat num", "shading method, not wireframe: 0=smooth 1=flat (0)"},
    {"-speed num", "speed of waves (0.05)"},
    {"-rotationx num", "speed of xrotation (0.01)"},
    {"-rotationy num", "speed of yrotation (0.00)"},
    {"-rotationz num", "speed of zrotation (0.10)"},
    {"-waves num", "number of simultanious waves (3)"},
    {"-waveChange num", "number of cyles for a wave to change (50)"},
    {"-waveHeight num", "height of waves (1.0)"},
    {"-waveFreq num", "max frequency of a wave (3.0)"},
    {"-zoom num", "camera control (1.0)"},
};

ModeSpecOpt gflux_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   gflux_description =
{"gflux", "init_gflux", "draw_gflux", "release_gflux",
 "draw_gflux", "init_gflux", NULL, &gflux_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "Gflux: an OpenGL gflux", 0, NULL};
#endif

/* structure for holding the gflux data */
typedef struct {
    ModeInfo *modeinfo;
    int screen_width, screen_height;
    GLXContext *glx_context;
    Window window;
    XColor fg, bg;
#define MAXWAVES 10   /* should be dynamic    */
    double wa[MAXWAVES];
    double freq[MAXWAVES];
    double dispy[MAXWAVES];
    double dispx[MAXWAVES];
    GLfloat colour[3];
    GLuint texName;
    GLfloat tex_xscale;
    GLfloat tex_yscale;
    XRectangle img_geom;
    int img_width, img_height;
    void (*drawFunc)(void);
} gfluxstruct;
static gfluxstruct *gflux = NULL;

/* prototypes */
void initLighting(void);
void grabTexture(void);
void createTexture(void);
void displaySolid(void);            /* drawFunc implementations */
void displayLight(void);
void displayTexture(void);
void displayWire(void);
void calcGrid(void);
double getGrid(double,double,double);

/* as macro for speed */
/* could do with colour cycling here */
/* void genColour(double);*/
#define genColour(X) \
{\
    gflux->colour[0] = 0.0;\
    gflux->colour[1] = 0.5+0.5*(X);\
    gflux->colour[2] = 0.5-0.5*(X);\
}

/* BEGINNING OF FUNCTIONS */


Bool
gflux_handle_event (ModeInfo *mi, XEvent *event)
{
  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      button_down_p = True;
      gltrackball_start (trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (trackball, event->xbutton.button, 10,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           button_down_p)
    {
      gltrackball_track (trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


static void
userRot(void)
{
  gltrackball_rotate (trackball);
}

/* draw the gflux once */
void draw_gflux(ModeInfo * mi)
{
    gfluxstruct *gp = &gflux[MI_SCREEN(mi)];
    Display    *display = MI_DISPLAY(mi);
    Window      window = MI_WINDOW(mi);

    if (!gp->glx_context) return;

    glXMakeCurrent(display, window, *(gp->glx_context));

    calcGrid();
    gflux->drawFunc();
    if (mi->fps_p) do_fps (mi);
    glXSwapBuffers(display, window);
}


/* reset the projection matrix */
void resetProjection(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-_zoom,_zoom,-0.8*_zoom,0.8*_zoom,2,6);
    glTranslatef(0.0,0.0,-4.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* Standard reshape function */
void
reshape_gflux(ModeInfo *mi, int width, int height)
{
    glViewport( 0, 0, width, height );
    resetProjection();
}


/* main OpenGL initialization routine */
void initializeGL(ModeInfo *mi, GLsizei width, GLsizei height) 
{
  reshape_gflux(mi, width, height);
  glViewport( 0, 0, width, height ); 

  gflux->tex_xscale = 1.0;  /* maybe changed later */
  gflux->tex_yscale = 1.0;

  switch(_draw) {
    case solid :
      gflux->drawFunc = (displaySolid);
      glEnable(GL_DEPTH_TEST);
    break;
    case light :
      gflux->drawFunc = (displayLight);
      glEnable(GL_DEPTH_TEST);
      initLighting();
	break;
	case checker :
      gflux->drawFunc = (displayTexture);
      glEnable(GL_DEPTH_TEST);
      createTexture();
      initLighting();
    break;
	case grab :
      gflux->drawFunc = (displayTexture);
      glEnable(GL_DEPTH_TEST);
      grabTexture();
      initLighting();
    break;
    case wire :
	default :
      gflux->drawFunc = (displayWire);
      glDisable(GL_DEPTH_TEST);
    break;
  }

  if(_flat) glShadeModel(GL_FLAT);
  else glShadeModel(GL_SMOOTH);

}


/* xgflux initialization routine */
void init_gflux(ModeInfo * mi)
{
    int screen = MI_SCREEN(mi);
    gfluxstruct *gp;

    if (gflux == NULL) {
        if ((gflux = (gfluxstruct *) 
                 calloc(MI_NUM_SCREENS(mi), sizeof (gfluxstruct))) == NULL)
            return;
    }
    gp = &gflux[screen];

    trackball = gltrackball_init ();

    {
      char *s = get_string_resource ("mode", "Mode");
      if (!s || !*s)                       _draw = wire;
      else if (!strcasecmp (s, "wire"))    _draw = wire;
      else if (!strcasecmp (s, "solid"))   _draw = solid;
      else if (!strcasecmp (s, "light"))   _draw = light;
      else if (!strcasecmp (s, "checker")) _draw = checker;
      else if (!strcasecmp (s, "grab"))    _draw = grab;
      else
        {
          fprintf (stderr,
                   "%s: mode must be one of: wire, solid, "
                   "light, checker, or grab; not \"%s\"\n",
                   progname, s);
          exit (1);
        }
    }

    gp->modeinfo = mi;
    gp->window = MI_WINDOW(mi);
    if ((gp->glx_context = init_GL(mi)) != NULL) {
        reshape_gflux(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
        initializeGL(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
    } else {
        MI_CLEARWINDOW(mi);
    }
}

/* cleanup code */
void release_gflux(ModeInfo * mi)
{
    if(gflux->glx_context!=NULL) free(gflux->glx_context);
    if (gflux != NULL) {
        (void) free((void *) gflux);
        gflux = NULL;
    }
    FreeAllGL(mi);
}



void createTexture(void)
{
  int size = 4;
  unsigned int data[] = { 0xFFFFFFFF, 0xAAAAAAAA, 0xFFFFFFFF, 0xAAAAAAAA,
                          0xAAAAAAAA, 0xFFFFFFFF, 0xAAAAAAAA, 0xFFFFFFFF,
                          0xFFFFFFFF, 0xAAAAAAAA, 0xFFFFFFFF, 0xAAAAAAAA,
                          0xAAAAAAAA, 0xFFFFFFFF, 0xAAAAAAAA, 0xFFFFFFFF };

  gflux->tex_xscale = size;
  gflux->tex_yscale = size;

  glGenTextures (1, &gflux->texName);
  glBindTexture (GL_TEXTURE_2D, gflux->texName);

  glTexImage2D (GL_TEXTURE_2D, 0, 3, size, size, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}


void
grabTexture(void)
{
  Bool mipmap_p = True;
  int iw, ih, tw, th;

  if (MI_IS_WIREFRAME(gflux->modeinfo))
    return;

  if (! screen_to_texture (gflux->modeinfo->xgwa.screen,
                           gflux->modeinfo->window, 0, 0, mipmap_p,
                           NULL, &gflux->img_geom, &iw, &ih, &tw, &th))
    exit (1);

  gflux->tex_xscale =  (GLfloat) iw / tw;
  gflux->tex_yscale = -(GLfloat) ih / th;
  gflux->img_width  = iw;
  gflux->img_height = ih;
   
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   (mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
}


void initLighting(void)
{
    static float ambientA[] = {0.0, 0.0, 0.0, 1.0};
    static float diffuseA[] = {1.0, 1.0, 1.0, 1.0};
    static float positionA[] = {5.0, 5.0, 15.0, 1.0};

    static float front_mat_shininess[] = {30.0};
    static float front_mat_specular[] = {0.5, 0.5, 0.5, 1.0};

    static float mat_diffuse[] = {0.5, 0.5, 0.5, 1.0};

    glMaterialfv(GL_FRONT, GL_SHININESS, front_mat_shininess);
    glMaterialfv(GL_FRONT, GL_SPECULAR, front_mat_specular);

    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientA);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseA);
    glLightfv(GL_LIGHT0, GL_POSITION, positionA);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE,1);

    glEnable(GL_NORMALIZE);         /* would it be faster ...   */
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
}

/************************************/
/* draw implementations             */
/* somewhat inefficient since they  */
/* all calculate previously         */
/* calculated values again          */
/* storing the values in an array   */
/* is a posibility                  */
/************************************/
void displayTexture(void)
{
    static double time = 0.0;
    static double anglex = 0.0;
    static double angley = 0.0;
    static double anglez = 0.0;

    double x,y,u,v;
    double z;
    double dx = 2.0/((double)_squares);
    double dy = 2.0/((double)_squares);

    double du = 2.0/((double)_squares);
    double dv = 2.0/((double)_squares);

    double xs = gflux->tex_xscale;
    double ys = gflux->tex_yscale;

    double minx, miny, maxx, maxy;
    double minu, minv;

#if 0
    minx = (GLfloat) gflux->img_geom.x / gflux->img_width;
    miny = (GLfloat) gflux->img_geom.y / gflux->img_height;
    maxx = ((GLfloat) (gflux->img_geom.x + gflux->img_geom.width) /
            gflux->img_width);
    maxy = ((GLfloat) (gflux->img_geom.y + gflux->img_geom.height) /
            gflux->img_height);
    minu = minx;
    minv = miny;
    minx = (minx * 2) - 1;
    miny = (miny * 2) - 1;
    maxx = (maxx * 2) - 1;
    maxy = (maxy * 2) - 1;
#else
    minx = -1;
    miny = -1;
    maxx = 1;
    maxy = 1;
    minv = 0;
    minu = 0;
#endif

	glMatrixMode (GL_TEXTURE);
	glLoadIdentity ();
	glTranslatef(-1,-1,0);
	glScalef(0.5,0.5,1);
	glMatrixMode (GL_MODELVIEW);

    glLoadIdentity();
    userRot();
    glRotatef(anglex,1,0,0);
    glRotatef(angley,0,1,0);
    glRotatef(anglez,0,0,1);
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);

    clear_gl_error();
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBindTexture(GL_TEXTURE_2D, gflux->texName);
    check_gl_error("texture binding");

	glColor3f(0.5,0.5,0.5);
 
    for(x = minx, u = minu; x < maxx - 0.01; x += dx, u += du) {
        glBegin(GL_QUAD_STRIP);
        for (y = miny, v = minv; y <= maxy + 0.01; y += dy, v += dv) {
            z = getGrid(x,y,time);
            glTexCoord2f(u*xs,v*ys);
            glNormal3f(
                getGrid(x+dx,y,time)-getGrid(x-dx,y,time),
                getGrid(x,y+dy,time)-getGrid(x,y-dy,time),
                1
            );
            glVertex3f(x,y,z);

            z = getGrid(x+dx,y,time);
            glTexCoord2f((u+du)*xs,v*ys);
            glNormal3f(
                getGrid(x+dx+dx,y,time)-getGrid(x,y,time),
                getGrid(x+dx,y+dy,time)-getGrid(x+dx,y-dy,time),
                1
            );
            glVertex3f(x+dx,y,z);
        }
        glEnd();
    }

    /* Draw a border around the grid.
     */
    glColor3f(0.4, 0.4, 0.4);
    glDisable(GL_TEXTURE_2D);
    glEnable (GL_LINE_SMOOTH);

    glBegin(GL_LINE_LOOP);
    y = miny;
    for (x = minx; x <= maxx; x += dx)
      glVertex3f (x, y, getGrid (x, y, time));
    x = maxx;
    for (y = miny; y <= maxy; y += dy)
      glVertex3f (x, y, getGrid (x, y, time));
    y = maxy;
    for (x = maxx; x >= minx; x -= dx)
      glVertex3f (x, y, getGrid (x, y, time));
    x = minx;
    for (y = maxy; y >= miny; y -= dy)
      glVertex3f (x, y, getGrid (x, y, time));

    glEnd();
    glEnable(GL_TEXTURE_2D);

    if (! button_down_p) {
      time -= _speed;
      anglex -= _rotationx;
      angley -= _rotationy;
      anglez -= _rotationz;
    }
}
void displaySolid(void)
{
    static double time = 0.0;
    static double anglex = 0.0;
    static double angley = 0.0;
    static double anglez = 0.0;

    double x,y;
    double z;
    double dx = 2.0/((double)_squares);
    double dy = 2.0/((double)_squares);

    glLoadIdentity();
    glRotatef(anglex,1,0,0);
    glRotatef(angley,0,1,0);
    glRotatef(anglez,0,0,1);
    userRot();
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    for(x=-1;x<0.9999;x+=dx) {
        glBegin(GL_QUAD_STRIP);
        for(y=-1;y<=1;y+=dy) {
            z = getGrid(x,y,time);
            genColour(z);
            glColor3fv(gflux->colour);
            glVertex3f(x,y,z);

            z = getGrid(x+dx,y,time);
            genColour(z);
            glColor3fv(gflux->colour);
            glVertex3f(x+dx,y,z);
        }
        glEnd();
    }

    if (! button_down_p) {
      time -= _speed;
      anglex -= _rotationx;
      angley -= _rotationy;
      anglez -= _rotationz;
    }

}

void displayLight(void)
{
    static double time = 0.0;
    static double anglex = 0.0;
    static double angley = 0.0;
    static double anglez = 0.0;

    double x,y;
    double z;
    double dx = 2.0/((double)_squares);
    double dy = 2.0/((double)_squares);

    glLoadIdentity();
    glRotatef(anglex,1,0,0);
    glRotatef(angley,0,1,0);
    glRotatef(anglez,0,0,1);
    userRot();
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    for(x=-1;x<0.9999;x+=dx) {
        glBegin(GL_QUAD_STRIP);
        for(y=-1;y<=1;y+=dy) {
            z = getGrid(x,y,time);
            genColour(z);
            glColor3fv(gflux->colour);
            glNormal3f(
                getGrid(x+dx,y,time)-getGrid(x-dx,y,time),
                getGrid(x,y+dy,time)-getGrid(x,y-dy,time),
                1
            );
            glVertex3f(x,y,z);

            z = getGrid(x+dx,y,time);
            genColour(z);
            glColor3fv(gflux->colour);
            glNormal3f(
                getGrid(x+dx+dx,y,time)-getGrid(x,y,time),
                getGrid(x+dx,y+dy,time)-getGrid(x+dx,y-dy,time),
                1
            );
            glVertex3f(x+dx,y,z);
        }
        glEnd();
    }

    if (! button_down_p) {
      time -= _speed;
      anglex -= _rotationx;
      angley -= _rotationy;
      anglez -= _rotationz;
    }
}

void displayWire(void)
{
    static double time = 0.0;
    static double anglex = 0.0;
    static double angley = 0.0;
    static double anglez = 0.0;

    double x,y;
    double z;
    double dx1 = 2.0/((double)(_squares*_resolution)) - 0.00001;
    double dy1 = 2.0/((double)(_squares*_resolution)) - 0.00001;
    double dx2 = 2.0/((double)_squares) - 0.00001;
    double dy2 = 2.0/((double)_squares) - 0.00001;

    glLoadIdentity();
    glRotatef(anglex,1,0,0);
    glRotatef(angley,0,1,0);
    glRotatef(anglez,0,0,1);
    userRot();
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    for(x=-1;x<=1;x+=dx2) {
        glBegin(GL_LINE_STRIP);
        for(y=-1;y<=1;y+=dy1) {
            z = getGrid(x,y,time);
            genColour(z);
            glColor3fv(gflux->colour);
            glVertex3f(x,y,z);
        }
        glEnd();
    }
    for(y=-1;y<=1;y+=dy2) {
        glBegin(GL_LINE_STRIP);
        for(x=-1;x<=1;x+=dx1) {
            z = getGrid(x,y,time);
            genColour(z);
            glColor3fv(gflux->colour);
            glVertex3f(x,y,z);
        }
        glEnd();
    }

    if (! button_down_p) {
      time -= _speed;
      anglex -= _rotationx;
      angley -= _rotationy;
      anglez -= _rotationz;
    }
}

/* generates new ripples */
void calcGrid(void)
{
    static int counter=0;
    double tmp;
    static int newWave;

    if (button_down_p) return;

    tmp = 1.0/((double)_waveChange);
    if(!(counter%_waveChange)) {
        newWave = ((int)(counter*tmp))%_waves;
        gflux->dispx[newWave] = -frand(1.0);
        gflux->dispy[newWave] = -frand(1.0);
        gflux->freq[newWave] = _waveFreq * frand(1.0);
        gflux->wa[newWave] = 0.0;
    }
    counter++;
    gflux->wa[newWave] += tmp;
    gflux->wa[(newWave+1)%_waves] -= tmp;
}

/* returns a height for the grid given time and x,y space co-ords */
double getGrid(double x, double y, double a)
{
    register int i;
    double retval=0.0;
    double tmp;

    tmp = 1.0/((float)_waves);
    for(i=0;i<_waves;i++) {
      retval += gflux->wa[i] * tmp * sin( gflux->freq[i]
              * ( (x+gflux->dispx[i]) * (x+gflux->dispx[i]) 
                + (y+gflux->dispy[i]) * (y+gflux->dispy[i]) +a ) );
    }
    return(retval);
}

#endif

