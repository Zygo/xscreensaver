/* sballs --- balls spinning like crazy in GL */

#if 0
static const char sccsid[] = "@(#)sballs.c	5.02 2001/03/10 xlockmore";
#endif

/* Copyright (c) E. Lassauge, 2001. */

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
 * The original code for this mode was written by 
 * Mustata Bogdan (LoneRunner) <lonerunner@planetquake.com>
 * and can be found at http://www.cfxweb.net/lonerunner/
 *
 * Eric Lassauge  (November-07-2000) <lassauge@users.sourceforge.net>
 * 				    http://lassauge.free.fr/linux.html
 *
 * REVISION HISTORY:
 *
 * E.Lassauge - 03-Oct-2001:
 *      - minor bugfixes - get ready for xscreensaver
 * E.Lassauge - 09-Mar-2001:
 *      - get rid of my framerate options to use showfps
 * E.Lassauge - 28-Nov-2000:
 *      - add handling of polyhedrons (like in ico)
 *      - modified release part to add freeing of GL objects
 * E.Lassauge - 14-Nov-2000:
 *      - use new common xpm_to_ximage function
 *
 */

#ifdef STANDALONE	/* xscreensaver mode */
#define DEFAULTS 	"*delay: 	30000 \n" \
 			"*size: 	    0 \n" \
			"*cycles:	    4 \n" \
 			"*showFPS: 	False \n" \
 			"*wireframe:  	False \n" \

# define refresh_sballs 0
#define MODE_sballs
#include "xlockmore.h"		/* from the xscreensaver distribution */
#include "gltrackball.h"
#else				/* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "visgl.h"
#endif				/* !STANDALONE */

#ifdef MODE_sballs

#define MINSIZE 	32	/* minimal viewport size */
#define FRAME           50      /* frame count interval */
#define MAX_OBJ		8	/* number of 3D objects */

#if defined( USE_XPM ) || defined( USE_XPMINC ) || defined( STANDALONE )
/* USE_XPM & USE_XPMINC in xlock mode ; HAVE_XPM in xscreensaver mode */
# include "xpm-ximage.h"
# define I_HAVE_XPM

# ifdef STANDALONE

#  ifdef __GNUC__
   __extension__ /* don't warn about "string length is greater than the length
                    ISO C89 compilers are required to support" when including
                    the following XPM file... */
#  endif
#  include "../images/sball.xpm"
#  ifdef __GNUC__
   __extension__
#  endif
#  include "../images/sball-bg.xpm"
# else /* !STANDALONE */
#  include "pixmaps/sball.xpm"
#  include "pixmaps/sball-bg.xpm"
# endif /* !STANDALONE */
#endif /* HAVE_XPM */

/* Manage option vars */
#define DEF_TEXTURE	"True"
#define DEF_OBJECT	"0"
static Bool do_texture;
static int  object;
static int  spheres;

static XrmOptionDescRec opts[] = {
    {"-texture", ".sballs.texture", XrmoptionNoArg, "on"},
    {"+texture", ".sballs.texture", XrmoptionNoArg, "off"},
    {"-object", ".sballs.object", XrmoptionSepArg, 0},

};

static argtype vars[] = {
    {&do_texture,    "texture",    "Texture",    DEF_TEXTURE,    t_Bool},
    {&object,        "object",     "Object",     DEF_OBJECT,     t_Int},

};

static OptionStruct desc[] = {
    /*{"-count spheres", "set number of spheres"},*/
    /*{"-cycles speed", "set ball speed value"},*/
    {"-/+texture", "turn on/off texturing"},
    {"-object num", "number of the 3D object (0 means random)"},
};

ENTRYPOINT ModeSpecOpt sballs_opts =
 { sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc };

#ifdef USE_MODULES
ModStruct sballs_description =
    { "sballs", "init_sballs", "draw_sballs", "release_sballs",
    "draw_sballs", "change_sballs", (char *) NULL, &sballs_opts,
    /*
    delay,count,cycles,size,ncolors,sat
     */
    10000, 0, 10, 400, 64, 1.0, "",
    "balls spinning like crazy in GL", 0, NULL
};
#endif /* USE_MODULES */

/* misc types and defines */
#define vinit(a,i,j,k) {\
  (a)[0]=i;\
  (a)[1]=j;\
  (a)[2]=k;\
}
typedef float vec_t;
typedef vec_t vec3_t[3];

#define MAX_BALLS	20

/* the mode struct, contains all per screen variables */
typedef struct {
    GLint WIDTH, HEIGHT;	/* display dimensions */
    GLXContext *glx_context;


    XImage *btexture;		/* back texture image bits */
    XImage *ftexture;		/* face texture image bits */
    GLuint backid;		/* back texture id: GL world */
    GLuint faceid;		/* face texture id: GL world */

    vec3_t eye;
    vec3_t rotm;
    int speed;
    float radius[MAX_BALLS];

    trackball_state *trackball;
    Bool button_down_p;

} sballsstruct;

/* array of sballsstruct indexed by screen number */
static sballsstruct *sballs = (sballsstruct *) NULL;

/* lights */
static const float LightAmbient[]=   { 1.0f, 1.0f, 1.0f, 1.0f };
static const float LightDiffuse[]=   { 1.0f, 1.0f, 1.0f, 1.0f };
static const float LightPosition[]=  { 0.0f, 0.0f, 4.0f, 1.0f };

/* structure of the polyhedras */
typedef struct {
        const char *longname;   /* long name of object */
        const char *shortname;  /* short name of object */
        int         numverts;   /* number of vertices */
        float	    radius;     /* radius */
        vec3_t      v[MAX_BALLS];/* the vertices */
} Polyinfo;

static const Polyinfo polygons[] =
{

/* 0: objtetra - structure values for tetrahedron */
        {
                "tetrahedron", "tetra",         /* long and short names */
                4,                        	/* number of vertices */
		0.8,
                {               		/* vertices (x,y,z) */
                        /* all points must be within radius 2 of the origin */
#define T 1.0
                        {T, T, T},
                        {T, -T, -T},
                        {-T, T, -T},
                        {-T, -T, T},
#undef T
                }
        },

/* 1: objcube - structure values for cube */

        {
                "hexahedron", "cube",           /* long and short names */
                8,       			/* number of vertices, edges, and faces */
		0.6,
                {               		/* vertices (x,y,z) */
                        /* all points must be within radius 2 of the origin */
#define T 1.0
                        {T, T, T},
                        {T, T, -T},
                        {T, -T, -T},
                        {T, -T, T},
                        {-T, T, T},
                        {-T, T, -T},
                        {-T, -T, -T},
                        {-T, -T, T},
#undef T
                }
        },

/* 2: objocta - structure values for octahedron */

        {
                "octahedron", "octa",   /* long and short names */
                6,			/* number of vertices */
		0.6,
                {               	/* vertices (x,y,z) */
                        /* all points must be within radius 2 of the origin */
#define T 1.5
                        {T, 0, 0},
                        {-T, 0, 0},
                        {0, T, 0},
                        {0, -T, 0},
                        {0, 0, T},
                        {0, 0, -T},
#undef T
                }
        },
/* 3: objdodec - structure values for dodecahedron */

        {
                "dodecahedron", "dodeca",       /* long and short names */
                20, 				/* number of vertices */
		0.35,
                {               		/* vertices (x,y,z) */
                        /* all points must be within radius 2 of the origin */
                        {0.000000, 0.500000, 1.000000},
                        {0.000000, -0.500000, 1.000000},
                        {0.000000, -0.500000, -1.000000},
                        {0.000000, 0.500000, -1.000000},
                        {1.000000, 0.000000, 0.500000},
                        {-1.000000, 0.000000, 0.500000},
                        {-1.000000, 0.000000, -0.500000},
                        {1.000000, 0.000000, -0.500000},
                        {0.500000, 1.000000, 0.000000},
                        {-0.500000, 1.000000, 0.000000},
                        {-0.500000, -1.000000, 0.000000},
                        {0.500000, -1.000000, 0.000000},
                        {0.750000, 0.750000, 0.750000},
                        {-0.750000, 0.750000, 0.750000},
                        {-0.750000, -0.750000, 0.750000},
                        {0.750000, -0.750000, 0.750000},
                        {0.750000, -0.750000, -0.750000},
                        {0.750000, 0.750000, -0.750000},
                        {-0.750000, 0.750000, -0.750000},
                        {-0.750000, -0.750000, -0.750000},
                }
        },

/* 4: objicosa - structure values for icosahedron */

        {
                "icosahedron", "icosa",         /* long and short names */
                12, 				/* number of vertices */
		0.4,
                {               		/* vertices (x,y,z) */
                        /* all points must be within radius 2 of the origin */
                        {0.00000000, 0.00000000, -0.95105650},
                        {0.00000000, 0.85065080, -0.42532537},
                        {0.80901698, 0.26286556, -0.42532537},
                        {0.50000000, -0.68819095, -0.42532537},
                        {-0.50000000, -0.68819095, -0.42532537},
                        {-0.80901698, 0.26286556, -0.42532537},
                        {0.50000000, 0.68819095, 0.42532537},
                        {0.80901698, -0.26286556, 0.42532537},
                        {0.00000000, -0.85065080, 0.42532537},
                        {-0.80901698, -0.26286556, 0.42532537},
                        {-0.50000000, 0.68819095, 0.42532537},
                        {0.00000000, 0.00000000, 0.95105650}
                }
        },

/* 5: objplane - structure values for plane */

        {
                "plane", "plane",       /* long and short names */
                4,			/* number of vertices */
		0.7,
                {               	/* vertices (x,y,z) */
                        /* all points must be within radius 2 of the origin */
#define T 1.1
                        {T, 0, 0},
                        {-T, 0, 0},
                        {0, T, 0},
                        {0, -T, 0},
#undef T
                }
        },

/* 6: objpyr - structure values for pyramid */

        {
                "pyramid", "pyramid",   /* long and short names */
                5,			/* number of vertices */ 
		0.5,
                {               	/* vertices (x,y,z) */
                        /* all points must be within radius 1 of the origin */
#define T 1.0
                        {T, 0, 0},
                        {-T, 0, 0},
                        {0, T, 0},
                        {0, -T, 0},
                        {0, 0, T},
#undef T
                }
        },

/* 7: objstar - structure values for octahedron star (stellated octahedron?) */
        {
                "star", "star", /* long and short names */
                8,		/* number of vertices */
		0.7,
                {               /* vertices (x,y,z) */
                        /* all points must be within radius 1 of the origin */
#define T 0.9
                        {T, T, T},
                        {T, -T, -T},
                        {-T, T, -T},
                        {-T, -T, T},
                        {-T, -T, -T},
                        {-T, T, T},
                        {T, -T, T},
                        {T, T, -T},
#undef T
                }
        },

};




/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Misc funcs.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */


/* initialise textures */
static void inittextures(ModeInfo * mi)
{
    sballsstruct *sb = &sballs[MI_SCREEN(mi)];

#if defined( I_HAVE_XPM )
    if (do_texture) {

	glGenTextures(1, &sb->backid);
#ifdef HAVE_GLBINDTEXTURE
	glBindTexture(GL_TEXTURE_2D, sb->backid);
#endif /* HAVE_GLBINDTEXTURE */

        sb->btexture = xpm_to_ximage(MI_DISPLAY(mi),
                                     MI_VISUAL(mi),
                                     MI_COLORMAP(mi),
                                     sball_bg);
	if (!(sb->btexture)) {
	    (void) fprintf(stderr, "Error reading the background texture.\n");
            glDeleteTextures(1, &sb->backid);
            do_texture = False;
            sb->faceid = 0;       /* default textures */
            sb->backid = 0;
	    return;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        clear_gl_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		     sb->btexture->width, sb->btexture->height, 0,
		     GL_RGBA,
                     /* GL_UNSIGNED_BYTE, */
                     GL_UNSIGNED_INT_8_8_8_8_REV,
                     sb->btexture->data);
        check_gl_error("texture");

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glGenTextures(1, &sb->faceid);
#ifdef HAVE_GLBINDTEXTURE
	glBindTexture(GL_TEXTURE_2D, sb->faceid);
#endif /* HAVE_GLBINDTEXTURE */

        sb->ftexture = xpm_to_ximage(MI_DISPLAY(mi),
                                     MI_VISUAL(mi),
                                     MI_COLORMAP(mi),
                                     sball);
	if (!(sb->ftexture)) {
	    (void) fprintf(stderr, "Error reading the face texture.\n");
            glDeleteTextures(1, &sb->faceid);
	    sb->faceid = 0;
	    return;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        clear_gl_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		     sb->ftexture->width, sb->ftexture->height, 0,
		     GL_RGBA,
                     /* GL_UNSIGNED_BYTE, */
                     GL_UNSIGNED_INT_8_8_8_8_REV,
                     sb->ftexture->data);
        check_gl_error("texture");

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    else
    {
        sb->faceid = 0;       /* default textures */
        sb->backid = 0;
    }
#else /* !I_HAVE_XPM */
  do_texture = False;
  sb->faceid = 0;       /* default textures */
  sb->backid = 0;
#endif /* !I_HAVE_XPM */
}

static void drawSphere(ModeInfo * mi,int sphere_num)
{
  sballsstruct *sb = &sballs[MI_SCREEN(mi)];
  float x = polygons[object].v[sphere_num][0];
  float y = polygons[object].v[sphere_num][1];
  float z = polygons[object].v[sphere_num][2];
  int numMajor = 15;
  int numMinor = 30;
  float radius = sb->radius[sphere_num];
  double majorStep = (M_PI / numMajor);
  double minorStep = (2.0 * M_PI / numMinor);
  int i, j;

  glPushMatrix();
  glTranslatef(x, y, z);

  glColor4f(1, 1, 1, 1);
  for (i = 0; i < numMajor; ++i)
  {
    double a = i * majorStep;
    double b = a + majorStep;
    double r0 = radius * sin(a);
    double r1 = radius * sin(b);
    GLfloat z0 = radius * cos(a);
    GLfloat z1 = radius * cos(b);

    glBegin(MI_IS_WIREFRAME(mi) ? GL_LINE_STRIP: GL_TRIANGLE_STRIP);
    for (j = 0; j <= numMinor; ++j)
        {
      double c = j * minorStep;
      GLfloat x = cos(c);
      GLfloat y = sin(c);

      glNormal3f((x * r0) / radius, (y * r0) / radius, z0 / radius);
      glTexCoord2f(j / (GLfloat) numMinor, i / (GLfloat) numMajor);
      glVertex3f(x * r0, y * r0, z0);

      glNormal3f((x * r1) / radius, (y * r1) / radius, z1 / radius);
      glTexCoord2f(j / (GLfloat) numMinor, (i + 1) / (GLfloat) numMajor);
      glVertex3f(x * r1, y * r1, z1);

      mi->polygon_count++;
    }
    glEnd();
  }

  glPopMatrix();
}

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    GL funcs.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

#ifndef STANDALONE
static void Reshape(ModeInfo * mi)
#else
ENTRYPOINT void reshape_sballs(ModeInfo * mi, int width, int height)
#endif
{

    sballsstruct *sb = &sballs[MI_SCREEN(mi)];
    int size = MI_SIZE(mi);

    /* Viewport is specified size if size >= MINSIZE && size < screensize */
    if (size <= 1) {
        sb->WIDTH = MI_WIDTH(mi);
        sb->HEIGHT = MI_HEIGHT(mi);
    } else if (size < MINSIZE) {
        sb->WIDTH = MINSIZE;
        sb->HEIGHT = MINSIZE;
    } else {
        sb->WIDTH = (size > MI_WIDTH(mi)) ? MI_WIDTH(mi) : size;
        sb->HEIGHT = (size > MI_HEIGHT(mi)) ? MI_HEIGHT(mi) : size;
    }
    glViewport((MI_WIDTH(mi) - sb->WIDTH) / 2, (MI_HEIGHT(mi) - sb->HEIGHT) / 2, sb->WIDTH, sb->HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(55.0, (float)sb->WIDTH / (float) sb->HEIGHT, 1.0, 300.0);

    glMatrixMode(GL_MODELVIEW);

}

static void Draw(ModeInfo * mi)
{
    sballsstruct *sb = &sballs[MI_SCREEN(mi)];
    int sphere;

    mi->polygon_count = 0;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();
    glEnable(GL_DEPTH_TEST);

    /* move eyes */
    glTranslatef (-sb->eye[0], -sb->eye[1], -sb->eye[2]);

    /* draw background */
    if (do_texture)
    {
       glEnable(GL_LIGHTING);
       glEnable(GL_TEXTURE_2D);
       glColor3f(1, 1, 1);
#ifdef HAVE_GLBINDTEXTURE
       glBindTexture(GL_TEXTURE_2D, sb->backid);
#endif /* HAVE_GLBINDTEXTURE */
    }
    else
    {
       glColor3f(0, 0, 0);
    }
    glBegin(GL_QUAD_STRIP);
    glNormal3f(0, 0, 1); glTexCoord2f(0,0); glVertex3f(8, 4.1, -4);
    glNormal3f(0, 0, 1); glTexCoord2f(0,1); glVertex3f(8, -4.1, -4);
    glNormal3f(0, 0, 1); glTexCoord2f(1,0); glVertex3f(-8, 4.1, -4);
    glNormal3f(0, 0, 1); glTexCoord2f(1,1); glVertex3f(-8, -4.1, -4);
    glEnd();
    mi->polygon_count++;

    gltrackball_rotate (sb->trackball);

    /* rotate the balls */
    glRotatef(sb->rotm[0], 1.0f, 0.0f, 0.0f);
    glRotatef(sb->rotm[1], 0.0f, 1.0f, 0.0f);
    glRotatef(sb->rotm[2], 0.0f, 0.0f, 1.0f);

    if (! sb->button_down_p) {
      sb->rotm[0] += sb->speed;
      sb->rotm[1] += -(sb->speed);
      sb->rotm[2] += 0;
    }

    /* draw the balls */
    if (do_texture)
#ifdef HAVE_GLBINDTEXTURE
       glBindTexture(GL_TEXTURE_2D, sb->faceid);
#endif /* HAVE_GLBINDTEXTURE */
    else
       glEnable(GL_LIGHTING);
    for (sphere=0;sphere<spheres;sphere++)
    {
        drawSphere(mi,sphere);
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    /* manage framerate display */
    if (MI_IS_FPS(mi)) do_fps (mi);
    glPopMatrix();
}


static void Init(ModeInfo * mi)
{
    sballsstruct *sb = &sballs[MI_SCREEN(mi)];
    int i;

    /* Default settings */
    if (MI_IS_WIREFRAME(mi))
	do_texture = False;
    if (do_texture)
    	inittextures(mi);
    else
    {
	sb->btexture = (XImage*) NULL;
	sb->ftexture = (XImage*) NULL;
    }

    vinit(sb->eye   ,0.0f, 0.0f, 6.0f);
    vinit(sb->rotm  ,0.0f, 0.0f, 0.0f);
    sb->speed = MI_CYCLES(mi);

    /* initialise object number */
    if ((object == 0) || (object > MAX_OBJ))
      object = NRAND(MAX_OBJ-1)+1;
    object--;

    /* initialise sphere number */
    spheres = MI_COUNT(mi);
    if (MI_COUNT(mi) > polygons[object].numverts)
	spheres = polygons[object].numverts;
    if (MI_COUNT(mi) < 1)
	spheres = polygons[object].numverts;
    /* initialise sphere radius */
    for(i=0; i < spheres;i++)
    {
#if RANDOM_RADIUS
	sb->radius[i] = ((float) LRAND() / (float) MAXRAND);
	if (sb->radius[i] < 0.3)
	    sb->radius[i] = 0.3;
	if (sb->radius[i] > 0.7)
	    sb->radius[i] = 0.7;
#else
	sb->radius[i] = polygons[object].radius;
#endif
    }

    if (MI_IS_DEBUG(mi)) {
	(void) fprintf(stderr,
		       "%s:\n\tobject=%s\n\tspheres=%d\n\tspeed=%d\n\ttexture=%s\n",
		       MI_NAME(mi),
		       polygons[object].shortname,
		       spheres,
		       (int) MI_CYCLES(mi),
		       do_texture ? "on" : "off"
			);
    }

        glLightfv(GL_LIGHT1, GL_AMBIENT, LightAmbient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);
        glLightfv(GL_LIGHT1, GL_POSITION,LightPosition);
        glEnable(GL_LIGHT1);

        glClearColor(0, 0, 0, 0);
}

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Xlock hooks.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *    Initialize sballs.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void init_sballs(ModeInfo * mi)
{
    sballsstruct *sb;

    if (sballs == NULL) {
	if ((sballs = (sballsstruct *) calloc(MI_NUM_SCREENS(mi),
					  sizeof(sballsstruct))) == NULL)
	    return;
    }
    sb = &sballs[MI_SCREEN(mi)];

    sb->trackball = gltrackball_init ();

    if ((sb->glx_context = init_GL(mi)) != NULL) {

#ifndef STANDALONE
	Reshape(mi); /* xlock mode */
#else
        reshape_sballs(mi,MI_WIDTH(mi),MI_HEIGHT(mi)); /* xscreensaver mode */
#endif
	glDrawBuffer(GL_BACK);
	Init(mi);

    } else {
	MI_CLEARWINDOW(mi);
    }
}

/*
 *-----------------------------------------------------------------------------
 *    Called by the mainline code periodically to update the display.
 *-----------------------------------------------------------------------------
 */
ENTRYPOINT void draw_sballs(ModeInfo * mi)
{
    Display *display = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);
    sballsstruct *sb;

    if (sballs == NULL)
	    return;
    sb = &sballs[MI_SCREEN(mi)];

    MI_IS_DRAWN(mi) = True;
    if (!sb->glx_context)
	return;

    glXMakeCurrent(display, window, *(sb->glx_context));
    Draw(mi);
#ifndef STANDALONE
    Reshape(mi); /* xlock mode */
#else
    reshape_sballs(mi,MI_WIDTH(mi),MI_HEIGHT(mi)); /* xscreensaver mode */
#endif

    glFinish();
    glXSwapBuffers(display, window);
}


/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed
 *      memory and X resources that we've alloc'ed.  Only called
 *      once, we must zap everything for every screen.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void release_sballs(ModeInfo * mi)
{
    int screen;

    if (sballs != NULL) {
	for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
	    sballsstruct *sb = &sballs[screen];
	    if (sb->btexture)
	    {
		glDeleteTextures(1,&sb->backid);
		XDestroyImage(sb->btexture);
	    }
	    if (sb->ftexture)
	    {
		glDeleteTextures(1,&sb->faceid);
		XDestroyImage(sb->ftexture);
	    }
	}
	(void) free((void *) sballs);
	sballs = (sballsstruct *) NULL;
    }
    FreeAllGL(mi);
}

ENTRYPOINT Bool
sballs_handle_event (ModeInfo *mi, XEvent *event)
{
  sballsstruct *sb = &sballs[MI_SCREEN(mi)];

  if (event->xany.type == ButtonPress &&
      event->xbutton.button == Button1)
    {
      sb->button_down_p = True;
      gltrackball_start (sb->trackball,
                         event->xbutton.x, event->xbutton.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }
  else if (event->xany.type == ButtonRelease &&
           event->xbutton.button == Button1)
    {
      sb->button_down_p = False;
      return True;
    }
  else if (event->xany.type == ButtonPress &&
           (event->xbutton.button == Button4 ||
            event->xbutton.button == Button5))
    {
      gltrackball_mousewheel (sb->trackball, event->xbutton.button, 5,
                              !!event->xbutton.state);
      return True;
    }
  else if (event->xany.type == MotionNotify &&
           sb->button_down_p)
    {
      gltrackball_track (sb->trackball,
                         event->xmotion.x, event->xmotion.y,
                         MI_WIDTH (mi), MI_HEIGHT (mi));
      return True;
    }

  return False;
}


#ifndef STANDALONE
ENTRYPOINT void change_sballs(ModeInfo * mi)
{
    sballsstruct *sb;

    if (sballs == NULL)
	    return;
    sb = &sballs[MI_SCREEN(mi)];

    if (!sb->glx_context)
	return;

    /* initialise object number */
    if ((object == 0) || (object > MAX_OBJ))
      object = NRAND(MAX_OBJ-1)+1;
    object--;

    /* correct sphere number */
    spheres = MI_COUNT(mi);
    if (MI_COUNT(mi) > polygons[object].numverts)
	spheres = polygons[object].numverts;
    if (MI_COUNT(mi) < 1)
	spheres = polygons[object].numverts;

    if (MI_IS_DEBUG(mi)) {
	(void) fprintf(stderr,
		       "%s:\n\tobject=%s\n\tspheres=%d\n\tspeed=%d\n\ttexture=%s\n",
		       MI_NAME(mi),
		       polygons[object].shortname,
		       spheres,
		       (int) MI_CYCLES(mi),
		       do_texture ? "on" : "off"
			);
    }
    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sb->glx_context));

}
#endif /* !STANDALONE */

XSCREENSAVER_MODULE ("SBalls", sballs)

#endif /* MODE_sballs */
