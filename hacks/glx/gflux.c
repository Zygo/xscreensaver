/* -*- Mode: C; tab-width: 4 -*- emacs friendly */

/* gflux - creates a fluctuating 3D grid 
 * requires OpenGL or MesaGL
 * 
 * Copyright (c) Josiah Pease, 2000
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
 */


/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */

#include <X11/Intrinsic.h>


#ifdef STANDALONE
# define PROGCLASS						"gflux"
# define HACK_INIT						init_gflux
# define HACK_DRAW						draw_gflux
# define gflux_opts				xlockmore_opts
#define DEFAULTS                        "*squares:      19      \n" \
                                        "*resolution:   0       \n" \
                                        "*draw:         0       \n" \
                                        "*flat:         0       \n" \
                                        "*speed:        0.05    \n" \
                                        "*rotationx:    0.01    \n" \
                                        "*rotationy:    0.0     \n" \
                                        "*rotationz:    0.1     \n" \
                                        "*waves:        3       \n" \
                                        "*waveChange:   50      \n" \
                                        "*waveHeight:   1.0     \n" \
                                        "*waveFreq:    3.0     \n" \
                                        "*zoom:         1.0     \n" 


# include "xlockmore.h"				/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL /* whole file */

#ifdef HAVE_XPM
# include <X11/xpm.h>
# ifndef PIXEL_ALREADY_TYPEDEFED
# define PIXEL_ALREADY_TYPEDEFED /* Sigh, Xmu/Drawing.h needs this... */
# endif
#endif

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

static enum {wire=0,solid,light,checker,textured} _draw; /* draw style */
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

#define WIDTH 320
#define HEIGHT 240

static XrmOptionDescRec opts[] = {
    {"-squares", ".gflux.squares", XrmoptionSepArg, (caddr_t) NULL},
    {"-resolution", ".gflux.resolution", XrmoptionSepArg, (caddr_t) NULL},
    {"-draw", ".gflux.draw", XrmoptionSepArg, (caddr_t) NULL},
    {"-flat", ".gflux.flat", XrmoptionSepArg, (caddr_t) NULL},
    {"-speed", ".gflux.speed", XrmoptionSepArg, (caddr_t) NULL},
    {"-rotationx", ".gflux.rotationx", XrmoptionSepArg, (caddr_t) NULL},
    {"-rotationy", ".gflux.rotationy", XrmoptionSepArg, (caddr_t) NULL},
    {"-rotationz", ".gflux.rotationz", XrmoptionSepArg, (caddr_t) NULL},
    {"-waves", ".gflux.waves", XrmoptionSepArg, (caddr_t) NULL},
    {"-waveChange", ".gflux.waveChange", XrmoptionSepArg, (caddr_t) NULL},
    {"-waveHeight", ".gflux.waveHeight", XrmoptionSepArg, (caddr_t) NULL},
    {"-waveFreq", ".gflux.waveFreq", XrmoptionSepArg, (caddr_t) NULL},
    {"-zoom", ".gflux.zoom", XrmoptionSepArg, (caddr_t) NULL},
};


static argtype vars[] = {
    {(caddr_t *) & _squares, "squares", "Squares", "19", t_Int},
    {(caddr_t *) & _resolution, "resolution", "Resolution", "4", t_Int},
    {(caddr_t *) & _draw, "draw", "Draw", "0", t_Int},
    {(caddr_t *) & _flat, "flat", "Flat", "0", t_Int},
    {(caddr_t *) & _speed, "speed", "Speed", "0.05", t_Float},
    {(caddr_t *) & _rotationx, "rotationx", "Rotationx", "0.01", t_Float},
    {(caddr_t *) & _rotationy, "rotationy", "Rotationy", "0.0", t_Float},
    {(caddr_t *) & _rotationz, "rotationz", "Rotationz", "0.1", t_Float},
    {(caddr_t *) & _waves, "waves", "Waves", "3", t_Int},
    {(caddr_t *) & _waveChange, "waveChange", "WaveChange", "50", t_Int},
    {(caddr_t *) & _waveHeight, "waveHeight", "WaveHeight", "1.0", t_Float},
    {(caddr_t *) & _waveFreq, "waveFreq", "WaveFreq", "3.0", t_Float},
    {(caddr_t *) & _zoom, "zoom", "Zoom", "1.0", t_Float},
};


static OptionStruct desc[] =
{
    {"-squares num", "size of grid in squares (19)"},
    {"-resolution num", "detail of lines making grid, wireframe only (4)"},
    {"-draw num", "draw method to use: 0=wireframe 1=solid 2=lit (0)"},
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
    int imageWidth;
    int imageHeight;
    GLubyte *image;
    GLint texName;
    void (*drawFunc)(void);
} gfluxstruct;
static gfluxstruct *gflux = NULL;

/* prototypes */
void initLighting(void);
void initTexture(void);
void loadTexture(void);
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
static void
reshape(int width, int height)
{
    glViewport( 0, 0, width, height );
    resetProjection();
}


/* main OpenGL initialization routine */
void initializeGL(GLsizei width, GLsizei height) 
{
  reshape(width, height);
  glViewport( 0, 0, width, height ); 
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
      initTexture();
      initLighting();
    break;
	case textured :
      gflux->drawFunc = (displayTexture);
      glEnable(GL_DEPTH_TEST);
      loadTexture();
      initTexture();
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

    gp->window = MI_WINDOW(mi);
    if ((gp->glx_context = init_GL(mi)) != NULL) {
        reshape(MI_WIDTH(mi), MI_HEIGHT(mi));
        initializeGL(MI_WIDTH(mi), MI_HEIGHT(mi));
    } else {
        MI_CLEARWINDOW(mi);
    }
}

/* cleanup code */
void release_gflux(ModeInfo * mi)
{
    if(gflux->image!=NULL) free(gflux->image);
    if(gflux->glx_context!=NULL) free(gflux->glx_context);
    if (gflux != NULL) {
        (void) free((void *) gflux);
        gflux = NULL;
    }
    FreeAllGL(mi);
}

/* loads several different types of PNM image from stdin */
#define presult(A,B,C) (*(result+(A)*(gflux->imageWidth)*4+(B)*4+(C)))
void loadTexture(void)
{
    int i, j, levels, width, height;
    int red,green,blue;
    char s[4];
    int ppmType=0;
    FILE *file = stdin;
    GLubyte *result;

    fgets(s,4,file);

    if(!strncmp(s,"P6",2)) ppmType=6;
    if(!strncmp(s,"P5",2)) ppmType=5;
    if(!strncmp(s,"P3",2)) ppmType=3;
    if(!strncmp(s,"P2",2)) ppmType=2;
    if(!ppmType)exit(1);

    while((i=getc(file))=='#')
    {
        while(getc(file)!='\n');
    }
    ungetc(i,file);

    fscanf(file,"%d %d %d",&width,&height,&levels);

    result = malloc(sizeof(GLubyte)*4*width*height);
    gflux->imageWidth = width;
	gflux->imageHeight = height;

    switch(ppmType) {
        case 2 :    /* ASCII grey */
            for(i=0;i<height;i++) {
                for(j=0;j<width;j++) {
                    fscanf(file,"%d",&red);
                    presult(j,i,0) = red;
                    presult(j,i,1) = red;
                    presult(j,i,2) = red;
                }
            }
            break;
        case 3 :    /* ASCII rgb */
            for(i=0;i<height;i++) {
                for(j=0;j<width;j++) {
                    fscanf(file,"%d %d %d",&red,&green,&blue);
                    presult(j,i,0) = red;
                    presult(j,i,1) = green;
                    presult(j,i,2) = blue;
                }
            }
            break;
        case 5 :    /* Binary grey */
            getc(file); /* seems nessessary */
            for(i=0;i<height;i++) {
                for(j=0;j<width;j++) {
                    red = getc(file);
                    presult(j,i,0) = red;
                    presult(j,i,1) = red;
                    presult(j,i,2) = red;
                }
            }
        break;
        case 6 :    /* Binary rgb */
            getc(file); /* seems nessessary */
            for(i=0;i<height;i++) {
                for(j=0;j<width;j++) {
                    red = getc(file);
                    green = getc(file);
                    blue = getc(file);
                    presult(j,i,0) = red;
                    presult(j,i,1) = green;
                    presult(j,i,2) = blue;
                }
            }
        break;
    }
	gflux->image = result;
}

/* creates an image for texture mapping */
void createTexture(void)
{
    int i,j,c;
    GLubyte *result;

	gflux->imageHeight = gflux->imageWidth = 8;

	result = malloc(sizeof(GLubyte)*4*gflux->imageHeight*gflux->imageWidth);
    for(i=0;i<gflux->imageHeight;i++) {
        for(j=0;j<gflux->imageWidth;j++) {
            c = (((i)%2 ^ (j)%2) ? 100 : 200 );
            presult(i,j,0) = (GLubyte) c;
            presult(i,j,1) = (GLubyte) c;
            presult(i,j,2) = (GLubyte) c;
            presult(i,j,3) = (GLubyte) 255;
        }
    }
	gflux->image = result;
}
#undef presult

/* specifies image as texture */    
void initTexture(void)
{
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &gflux->texName);
	glBindTexture(GL_TEXTURE_2D, gflux->texName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    clear_gl_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gflux->imageWidth,
			gflux->imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, gflux->image);
    check_gl_error("texture");
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

	glMatrixMode (GL_TEXTURE);
	glLoadIdentity ();
	glTranslatef(-1,-1,0);
	glScalef(0.5,0.5,1);
	glMatrixMode (GL_MODELVIEW);

    glLoadIdentity();
    glRotatef(anglex,1,0,0);
    glRotatef(angley,0,1,0);
    glRotatef(anglez,0,0,1);
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBindTexture(GL_TEXTURE_2D, gflux->texName);
	glColor3f(0.5,0.5,0.5);

    for(x=-1,u=0;x<=1;x+=dx,u+=dx) {
        glBegin(GL_QUAD_STRIP);
        for(y=-1,v=0;y<=1;y+=dy,v+=dx) {
            z = getGrid(x,y,time);
        /*  genColour(z);
            glColor3fv(gflux->colour);
        */  glTexCoord2f(u,v);
            glNormal3f(
                getGrid(x+dx,y,time)-getGrid(x-dx,y,time),
                getGrid(x,y+dy,time)-getGrid(x,y-dy,time),
                1
            );
            glVertex3f(x,y,z);

            z = getGrid(x+dx,y,time);
        /*  genColour(z);
            glColor3fv(gflux->colour);
        */  glTexCoord2f(u+dx,v);
            glNormal3f(
                getGrid(x+dx+dx,y,time)-getGrid(x,y,time),
                getGrid(x+dx,y+dy,time)-getGrid(x+dx,y-dy,time),
                1
            );
            glVertex3f(x+dx,y,z);
        }
        glEnd();
    }

    time -= _speed;
    anglex -= _rotationx;
    angley -= _rotationy;
    anglez -= _rotationz;
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
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    for(x=-1;x<=1;x+=dx) {
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

    time -= _speed;
    anglex -= _rotationx;
    angley -= _rotationy;
    anglez -= _rotationz;

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
    glScalef(1,1,(GLfloat)_waveHeight);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    for(x=-1;x<=1;x+=dx) {
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

    time -= _speed;
    anglex -= _rotationx;
    angley -= _rotationy;
    anglez -= _rotationz;
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

    time -= _speed;
    anglex -= _rotationx;
    angley -= _rotationy;
    anglez -= _rotationz;
}

/* generates new ripples */
void calcGrid(void)
{
    static int counter=0;
    double tmp;
    static int newWave;

    tmp = 1.0/((double)_waveChange);
    if(!(counter%_waveChange)) {
        newWave = ((int)(counter*tmp))%_waves;
        gflux->dispx[newWave] = 1.0 - ((double)random())/RAND_MAX;
        gflux->dispy[newWave] = 1.0 - ((double)random())/RAND_MAX;
        gflux->freq[newWave] = _waveFreq * ((float)random())/RAND_MAX;
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

