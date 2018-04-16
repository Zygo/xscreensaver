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
#define DEFAULTS                        "*delay:		20000\n" \
										"*showFPS:      False\n" \
                                        "*mode:         grab\n"  \
                                        "*useSHM:       True \n" \
										"*suppressRotationAnimation: True\n" \

# define free_gflux 0
# define release_gflux 0
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

#include <math.h>

#include "grab-ximage.h"
#include "gltrackball.h"


static enum {wire=0,solid,light,checker,grab} _draw;

# define DEF_SQUARES		"19"
# define DEF_RESOLUTION 	"4"
# define DEF_DRAW 			"2"
# define DEF_FLAT			"0"
# define DEF_SPEED			"0.05"
# define DEF_ROTATIONX		"0.01"
# define DEF_ROTATIONY		"0.0"
# define DEF_ROTATIONZ		"0.1"
# define DEF_WAVES			"3"
# define DEF_WAVE_CHANGE	"50"
# define DEF_WAVE_HEIGHT	"1.0"
# define DEF_WAVE_FREQ		"3.0"
# define DEF_ZOOM			"1.0"



static int _squares;                        /* grid size */
static int _resolution;                    /* wireframe resolution */
static int _flat;

static float _speed;
static float _rotationx;
static float _rotationy;
static float _rotationz;
static float _zoom;

static int _waves;
static int _waveChange;
static float _waveHeight;
static float _waveFreq;


#define WIDTH 320
#define HEIGHT 240

static XrmOptionDescRec opts[] = {
    {"-squares", ".gflux.squares", XrmoptionSepArg, 0},
    {"-resolution", ".gflux.resolution", XrmoptionSepArg, 0},
/*    {"-draw", ".gflux.draw", XrmoptionSepArg, 0},*/
    {"-mode", ".gflux.mode", XrmoptionSepArg, 0},
    {"-wireframe", ".gflux.mode", XrmoptionNoArg, "wire"},
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
    {&_squares, "squares", "Squares", DEF_SQUARES, t_Int},
    {&_resolution, "resolution", "Resolution", DEF_RESOLUTION, t_Int},
/*    {&_draw, "draw", "Draw", DEF_DRAW, t_Int},*/
    {&_flat, "flat", "Flat", DEF_FLAT, t_Int},
    {&_speed, "speed", "Speed", DEF_SPEED, t_Float},
    {&_rotationx, "rotationx", "Rotationx", DEF_ROTATIONX, t_Float},
    {&_rotationy, "rotationy", "Rotationy", DEF_ROTATIONY, t_Float},
    {&_rotationz, "rotationz", "Rotationz", DEF_ROTATIONZ, t_Float},
    {&_waves, "waves", "Waves", DEF_WAVES, t_Int},
    {&_waveChange, "waveChange", "WaveChange", DEF_WAVE_CHANGE, t_Int},
    {&_waveHeight, "waveHeight", "WaveHeight", DEF_WAVE_HEIGHT, t_Float},
    {&_waveFreq, "waveFreq", "WaveFreq", DEF_WAVE_FREQ, t_Float},
    {&_zoom, "zoom", "Zoom", DEF_ZOOM, t_Float},
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

ENTRYPOINT ModeSpecOpt gflux_opts = {countof(opts), opts, countof(vars), vars, desc};

#ifdef USE_MODULES
ModStruct   gflux_description =
{"gflux", "init_gflux", "draw_gflux", NULL,
 "draw_gflux", "init_gflux", NULL, &gflux_opts,
 1000, 1, 2, 1, 4, 1.0, "",
 "GFlux: an OpenGL gflux", 0, NULL};
#endif

/* structure for holding the gflux data */
typedef struct gfluxstruct {
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
    int (*drawFunc)(struct gfluxstruct *);

    trackball_state *trackball;
    Bool button_down_p;

    double time;
    double anglex;
    double angley;
    double anglez;

    int counter;
    int newWave;

    Bool mipmap_p;
    Bool waiting_for_image_p;

} gfluxstruct;
static gfluxstruct *gfluxes = NULL;

/* prototypes */
static void initLighting(void);
static void grabTexture(gfluxstruct *);
static void createTexture(gfluxstruct *);
static int displaySolid(gfluxstruct *);     /* drawFunc implementations */
static int displayLight(gfluxstruct *);
static int displayTexture(gfluxstruct *);
static int displayWire(gfluxstruct *);
static void calcGrid(gfluxstruct *);
static double getGrid(gfluxstruct *,double,double,double);

/* as macro for speed */
/* could do with colour cycling here */
/* void genColour(double);*/
#define genColour(X) \
{\
    gp->colour[0] = 0.0;\
    gp->colour[1] = 0.5+0.5*(X);\
    gp->colour[2] = 0.5-0.5*(X);\
}

/* BEGINNING OF FUNCTIONS */


ENTRYPOINT Bool
gflux_handle_event (ModeInfo *mi, XEvent *event)
{
  gfluxstruct *gp = &gfluxes[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, gp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &gp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      if (_draw == grab) {
        grabTexture(gp);
        return True;
      }
    }

  return False;
}


static void
userRot(gfluxstruct *gp)
{
  gltrackball_rotate (gp->trackball);
}

/* draw the gflux once */
ENTRYPOINT void draw_gflux(ModeInfo * mi)
{
    gfluxstruct *gp = &gfluxes[MI_SCREEN(mi)];
    Display    *display = MI_DISPLAY(mi);
    Window      window = MI_WINDOW(mi);

    if (!gp->glx_context) return;

    /* Just keep running before the texture has come in. */
    /* if (gp->waiting_for_image_p) return; */

    glXMakeCurrent(display, window, *(gp->glx_context));

    calcGrid(gp);
    mi->polygon_count = gp->drawFunc(gp);
    if (mi->fps_p) do_fps (mi);
    glXSwapBuffers(display, window);
}


/* Standard reshape function */
ENTRYPOINT void
reshape_gflux(ModeInfo *mi, int width, int height)
{
    glViewport( 0, 0, width, height );
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-_zoom,_zoom,-0.8*_zoom,0.8*_zoom,2,6);
    glTranslatef(0.0,0.0,-4.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

# ifdef HAVE_MOBILE	/* Keep it the same relative size when rotated. */
    {
      GLfloat h = MI_HEIGHT(mi) / (GLfloat) MI_WIDTH(mi);
      int o = (int) current_device_rotation();
      if (o != 0 && o != 180 && o != -180)
        glScalef (1/h, 1/h, 1/h);
    }
# endif
}


/* main OpenGL initialization routine */
static void initializeGL(ModeInfo *mi, GLsizei width, GLsizei height) 
{
  gfluxstruct *gp = &gfluxes[MI_SCREEN(mi)];

  reshape_gflux(mi, width, height);
  glViewport( 0, 0, width, height ); 

  gp->tex_xscale = 1.0;  /* maybe changed later */
  gp->tex_yscale = 1.0;

  switch(_draw) {
    case solid :
      gp->drawFunc = (displaySolid);
      glEnable(GL_DEPTH_TEST);
    break;
    case light :
      gp->drawFunc = (displayLight);
      glEnable(GL_DEPTH_TEST);
      initLighting();
	break;
	case checker :
      gp->drawFunc = (displayTexture);
      glEnable(GL_DEPTH_TEST);
      createTexture(gp);
      initLighting();
    break;
	case grab :
      gp->drawFunc = (displayTexture);
      glEnable(GL_DEPTH_TEST);
      grabTexture(gp);
      initLighting();
    break;
    case wire :
	default :
      gp->drawFunc = (displayWire);
      glDisable(GL_DEPTH_TEST);
    break;
  }

  if(_flat) glShadeModel(GL_FLAT);
  else glShadeModel(GL_SMOOTH);

}


/* xgflux initialization routine */
ENTRYPOINT void init_gflux(ModeInfo * mi)
{
    int screen = MI_SCREEN(mi);
    gfluxstruct *gp;

    MI_INIT(mi, gfluxes);
    gp = &gfluxes[screen];

    gp->trackball = gltrackball_init (True);

    gp->time = frand(1000.0);  /* don't run two screens in lockstep */

    {
      char *s = get_string_resource (mi->dpy, "mode", "Mode");
      if (!s || !*s)                       _draw = grab;
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


static void createTexture(gfluxstruct *gp)
{
  int size = 4;
  unsigned int data[] = { 0xFFFFFFFF, 0xAAAAAAAA, 0xFFFFFFFF, 0xAAAAAAAA,
                          0xAAAAAAAA, 0xFFFFFFFF, 0xAAAAAAAA, 0xFFFFFFFF,
                          0xFFFFFFFF, 0xAAAAAAAA, 0xFFFFFFFF, 0xAAAAAAAA,
                          0xAAAAAAAA, 0xFFFFFFFF, 0xAAAAAAAA, 0xFFFFFFFF };

  gp->tex_xscale = size;
  gp->tex_yscale = size;

  glGenTextures (1, &gp->texName);
  glBindTexture (GL_TEXTURE_2D, gp->texName);

  glTexImage2D (GL_TEXTURE_2D, 0, 3, size, size, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}


static void
image_loaded_cb (const char *filename, XRectangle *geometry,
                 int image_width, int image_height, 
                 int texture_width, int texture_height,
                 void *closure)
{
  gfluxstruct *gp = (gfluxstruct *) closure;
  gp->img_geom = *geometry;

  gp->tex_xscale =  (GLfloat) image_width  / texture_width;
  gp->tex_yscale = -(GLfloat) image_height / texture_height;
  gp->img_width  = image_width;
  gp->img_height = image_height;
   
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   (gp->mipmap_p ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));

  gp->waiting_for_image_p = False;
}

static void
grabTexture(gfluxstruct *gp)
{
  if (MI_IS_WIREFRAME(gp->modeinfo))
    return;

  gp->waiting_for_image_p = True;
  gp->mipmap_p = True;
  load_texture_async (gp->modeinfo->xgwa.screen, gp->modeinfo->window,
                      *gp->glx_context, 0, 0, gp->mipmap_p, gp->texName,
                      image_loaded_cb, gp);
}


static void initLighting(void)
{
    static const float ambientA[]  = {0.0, 0.0, 0.0, 1.0};
    static const float diffuseA[]  = {1.0, 1.0, 1.0, 1.0};
    static const float positionA[] = {5.0, 5.0, 15.0, 1.0};

    static const float front_mat_shininess[] = {30.0};
    static const float front_mat_specular[]  = {0.5, 0.5, 0.5, 1.0};

    static const float mat_diffuse[] = {0.5, 0.5, 0.5, 1.0};

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
static int displayTexture(gfluxstruct *gp)
{
    int polys = 0;
    double x,y,u,v;
    double z;
    double dx = 2.0/((double)_squares);
    double dy = 2.0/((double)_squares);

    double du = 2.0/((double)_squares);
    double dv = 2.0/((double)_squares);

    double xs = gp->tex_xscale;
    double ys = gp->tex_yscale;

    double minx, miny, maxx, maxy;
    double minu, minv;

#if 0
    minx = (GLfloat) gp->img_geom.x / gp->img_width;
    miny = (GLfloat) gp->img_geom.y / gp->img_height;
    maxx = ((GLfloat) (gp->img_geom.x + gp->img_geom.width) /
            gp->img_width);
    maxy = ((GLfloat) (gp->img_geom.y + gp->img_geom.height) /
            gp->img_height);
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
    userRot(gp);
    glRotatef(gp->anglex,1,0,0);
    glRotatef(gp->angley,0,1,0);
    glRotatef(gp->anglez,0,0,1);
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);

    clear_gl_error();
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBindTexture(GL_TEXTURE_2D, gp->texName);
    check_gl_error("texture binding");

	glColor3f(0.5,0.5,0.5);
 
    for(x = minx, u = minu; x < maxx - 0.01; x += dx, u += du) {
        glBegin(GL_QUAD_STRIP);
        for (y = miny, v = minv; y <= maxy + 0.01; y += dy, v += dv) {
            z = getGrid(gp,x,y,gp->time);
            glTexCoord2f(u*xs,v*ys);
            glNormal3f(
                getGrid(gp,x+dx,y,gp->time)-getGrid(gp, x-dx,y,gp->time),
                getGrid(gp,x,y+dy,gp->time)-getGrid(gp, x,y-dy,gp->time),
                1
            );
            glVertex3f(x,y,z);

            z = getGrid(gp,x+dx,y,gp->time);
            glTexCoord2f((u+du)*xs,v*ys);
            glNormal3f(
                getGrid(gp,x+dx+dx,y,gp->time)-getGrid(gp, x,y,gp->time),
                getGrid(gp,x+dx,y+dy,gp->time)-getGrid(gp, x+dx,y-dy,gp->time),
                1
            );
            glVertex3f(x+dx,y,z);
            polys++;
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
    for (x = minx; x <= maxx; x += dx) {
      glVertex3f (x, y, getGrid (gp, x, y, gp->time));
      polys++;
    }
    x = maxx;
    for (y = miny; y <= maxy; y += dy) {
      glVertex3f (x, y, getGrid (gp, x, y, gp->time));
      polys++;
    }
    y = maxy;
    for (x = maxx; x >= minx; x -= dx) {
      glVertex3f (x, y, getGrid (gp, x, y, gp->time));
      polys++;
    }
    x = minx;
    for (y = maxy; y >= miny; y -= dy) {
      glVertex3f (x, y, getGrid (gp, x, y, gp->time));
      polys++;
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);

    if (! gp->button_down_p) {
      gp->time -= _speed;
      gp->anglex -= _rotationx;
      gp->angley -= _rotationy;
      gp->anglez -= _rotationz;
    }
    return polys;
}

static int displaySolid(gfluxstruct *gp)
{
    int polys = 0;
    double x,y;
    double z;
    double dx = 2.0/((double)_squares);
    double dy = 2.0/((double)_squares);

    glLoadIdentity();
    glRotatef(gp->anglex,1,0,0);
    glRotatef(gp->angley,0,1,0);
    glRotatef(gp->anglez,0,0,1);
    userRot(gp);
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    for(x=-1;x<0.9999;x+=dx) {
        glBegin(GL_QUAD_STRIP);
        for(y=-1;y<=1;y+=dy) {
            z = getGrid(gp, x,y,gp->time);
            genColour(z);
            glColor3fv(gp->colour);
            glVertex3f(x,y,z);

            z = getGrid(gp, x+dx,y,gp->time);
            genColour(z);
            glColor3fv(gp->colour);
            glVertex3f(x+dx,y,z);
            polys++;
        }
        glEnd();
    }

    if (! gp->button_down_p) {
      gp->time -= _speed;
      gp->anglex -= _rotationx;
      gp->angley -= _rotationy;
      gp->anglez -= _rotationz;
    }

    return polys;
}

static int displayLight(gfluxstruct *gp)
{
    int polys = 0;
    double x,y;
    double z;
    double dx = 2.0/((double)_squares);
    double dy = 2.0/((double)_squares);

    glLoadIdentity();
    glRotatef(gp->anglex,1,0,0);
    glRotatef(gp->angley,0,1,0);
    glRotatef(gp->anglez,0,0,1);
    userRot(gp);
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    for(x=-1;x<0.9999;x+=dx) {
        glBegin(GL_QUAD_STRIP);
        for(y=-1;y<=1;y+=dy) {
            z = getGrid(gp, x,y,gp->time);
            genColour(z);
            glColor3fv(gp->colour);
            glNormal3f(
                getGrid(gp, x+dx,y,gp->time)-getGrid(gp, x-dx,y,gp->time),
                getGrid(gp, x,y+dy,gp->time)-getGrid(gp, x,y-dy,gp->time),
                1
            );
            glVertex3f(x,y,z);

            z = getGrid(gp, x+dx,y,gp->time);
            genColour(z);
            glColor3fv(gp->colour);
            glNormal3f(
                getGrid(gp, x+dx+dx,y,gp->time)-getGrid(gp, x,y,gp->time),
                getGrid(gp, x+dx,y+dy,gp->time)-getGrid(gp, x+dx,y-dy,gp->time),
                1
            );
            glVertex3f(x+dx,y,z);
            polys++;
        }
        glEnd();
    }

    if (! gp->button_down_p) {
      gp->time -= _speed;
      gp->anglex -= _rotationx;
      gp->angley -= _rotationy;
      gp->anglez -= _rotationz;
    }
    return polys;
}

static int displayWire(gfluxstruct *gp)
{
    int polys = 0;
    double x,y;
    double z;
    double dx1 = 2.0/((double)(_squares*_resolution)) - 0.00001;
    double dy1 = 2.0/((double)(_squares*_resolution)) - 0.00001;
    double dx2 = 2.0/((double)_squares) - 0.00001;
    double dy2 = 2.0/((double)_squares) - 0.00001;

    glLoadIdentity();
    glRotatef(gp->anglex,1,0,0);
    glRotatef(gp->angley,0,1,0);
    glRotatef(gp->anglez,0,0,1);
    userRot(gp);
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    for(x=-1;x<=1;x+=dx2) {
        glBegin(GL_LINE_STRIP);
        for(y=-1;y<=1;y+=dy1) {
            z = getGrid(gp, x,y,gp->time);
            genColour(z);
            glColor3fv(gp->colour);
            glVertex3f(x,y,z);
            polys++;
        }
        glEnd();
    }
    for(y=-1;y<=1;y+=dy2) {
        glBegin(GL_LINE_STRIP);
        for(x=-1;x<=1;x+=dx1) {
            z = getGrid(gp, x,y,gp->time);
            genColour(z);
            glColor3fv(gp->colour);
            glVertex3f(x,y,z);
            polys++;
        }
        glEnd();
    }

    if (! gp->button_down_p) {
      gp->time -= _speed;
      gp->anglex -= _rotationx;
      gp->angley -= _rotationy;
      gp->anglez -= _rotationz;
    }
    return polys;
}

/* generates new ripples */
static void calcGrid(gfluxstruct *gp)
{
    double tmp;

    if (gp->button_down_p) return;

    tmp = 1.0/((double)_waveChange);
    if(!(gp->counter%_waveChange)) {
        gp->newWave = ((int)(gp->counter*tmp))%_waves;
        gp->dispx[gp->newWave] = -frand(1.0);
        gp->dispy[gp->newWave] = -frand(1.0);
        gp->freq[gp->newWave] = _waveFreq * frand(1.0);
        gp->wa[gp->newWave] = 0.0;
    }
    gp->counter++;
    gp->wa[gp->newWave] += tmp;
    gp->wa[(gp->newWave+1)%_waves] -= tmp;
}

/* returns a height for the grid given time and x,y space co-ords */
static double getGrid(gfluxstruct *gp, double x, double y, double a)
{
    register int i;
    double retval=0.0;
    double tmp;

    tmp = 1.0/((float)_waves);
    for(i=0;i<_waves;i++) {
      retval += gp->wa[i] * tmp * sin( gp->freq[i]
              * ( (x+gp->dispx[i]) * (x+gp->dispx[i]) 
                + (y+gp->dispy[i]) * (y+gp->dispy[i]) +a ) );
    }
    return(retval);
}


XSCREENSAVER_MODULE ("GFlux", gflux)

#endif
