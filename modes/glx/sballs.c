/* sballs --- 8 balls spinning like crazy in GL */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)sballs.c	5.01 2001/03/09 xlockmore";
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
 * Eric Lassauge  (November-07-2000) <lassauge@mail.dotcom.fr>
 * 				    http://lassauge.free.fr/linux.html
 *
 * REVISION HISTORY:
 *
 * E.Lassauge - 09-Mar-2001:
 *      - get rid of my framerate options to use showfps
 * E.Lassauge - 28-Nov-2000:
 *      - add handling of polyhedrons (like in ico)
 *      - modified release part to add freeing of GL objects
 * E.Lassauge - 14-Nov-2000:
 *      - use new common xpm_to_ximage function
 *
 */

#ifdef STANDALONE
#define PROGCLASS "Sballs"
#define HACK_INIT init_sballs
#define HACK_DRAW draw_sballs
#define sballs xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*size: 0 \n" \
 "*object: 0\n" \	/* random object */
 "*trackmouse: False \n" \
 "*showfps: False \n" \
 "*wireframe:  False \n"

#include "xlockmore.h"		/* from the xscreensaver distribution */
#else				/* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "visgl.h"
#endif				/* !STANDALONE */
#include "iostuff.h"            /* getFont() */

#ifdef MODE_sballs

#define MINSIZE 	32	/* minimal viewport size */
#define FRAME           50      /* frame count interval */
#define MAX_OBJ		8	/* number of 3D objects */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include "xpm-ximage.h"

#ifdef STANDALONE
#include "../images/face.xpm"
#include "../images/back.xpm"
#else /* !STANDALONE */
#include "pixmaps/face.xpm"
#include "pixmaps/back.xpm"
#endif /* !STANDALONE */

/* Manage option vars */
#define DEF_TEXTURE	"True"
#define DEF_TRACKMOUSE  "False"
#define DEF_OBJECT	"2"
#define DEF_OBJECT_INDX	2
static Bool do_texture;
static Bool do_trackmouse;
static int  object;
static int  spheres;
static XFontStruct *mode_font = None;

static XrmOptionDescRec opts[] = {
    {(char *) "-texture", (char *) ".sballs.texture", XrmoptionNoArg, (caddr_t) "on"},
    {(char *) "+texture", (char *) ".sballs.texture", XrmoptionNoArg, (caddr_t) "off"},
    {(char *) "-trackmouse", (char *) ".sballs.trackmouse", XrmoptionNoArg, (caddr_t) "on"},
    {(char *) "+trackmouse", (char *) ".sballs.trackmouse", XrmoptionNoArg, (caddr_t) "off"},
    {(char *) "-object", (char *) ".sballs.object", XrmoptionSepArg, (caddr_t) NULL},

};

static argtype vars[] = {
    {(caddr_t *) & do_texture, (char *) "texture", (char *) "Texture", (char *) DEF_TEXTURE, t_Bool},
    {(caddr_t *) & do_trackmouse, (char *) "trackmouse", (char *) "TrackMouse", (char *) DEF_TRACKMOUSE, t_Bool},
    {(caddr_t *) & object, (char *) "object", (char *) "Object", (char *) DEF_OBJECT, t_Int},

};

static OptionStruct desc[] = {
    /*{(char *) "-count spheres", (char *) "set number of spheres"},*/
    /*{(char *) "-cycles speed", (char *) "set ball speed value"},*/
    {(char *) "-/+texture", (char *) "turn on/off texturing"},
    {(char *) "-/+trackmouse", (char *) "turn on/off the tracking of the mouse"},
    {(char *) "-object num", (char *) "number of the 3D object (0 means random)"},
};

ModeSpecOpt sballs_opts =
 { sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc };

#ifdef USE_MODULES
ModStruct sballs_description =
    { "sballs", "init_sballs", "draw_sballs", "release_sballs",
    "draw_sballs", "change_sballs", NULL, &sballs_opts,
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
    GLuint fontbase;            /* fontbase id: GL world */

    vec3_t eye;
    vec3_t rot;
    vec3_t rotm;
    int speed;
    float radius[MAX_BALLS];

    clock_t told;		/* frame timetag */
    int frcount;		/* frame counter */
    char frbuf[80];		/* frame value string */
} sballsstruct;

/* array of sballsstruct indexed by screen number */
static sballsstruct *sballs = (sballsstruct *) NULL;

/* lights */
static float LightAmbient[]=   { 1.0f, 1.0f, 1.0f, 1.0f };
static float LightDiffuse[]=   { 1.0f, 1.0f, 1.0f, 1.0f };
static float LightPosition[]=  { 0.0f, 0.0f, 4.0f, 1.0f };

/* structure of the polyhedras */
typedef struct {
        char       *longname;   /* long name of object */
        char       *shortname;  /* short name of object */
        int         numverts;   /* number of vertices */
        float	    radius;     /* radius */
        vec3_t      v[MAX_BALLS];/* the vertices */
} Polyinfo;

static Polyinfo polygons[] =
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
                "hexahedron", "cube",   	/* long and short names */
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

static void clamp(vec3_t v)
{
    int i;

    for (i = 0; i < 3; i ++)
        if (v[i] > 360 || v[i] < -360)
            v[i] = 0;
}

/* track the mouse in a joystick manner : not perfect but it works */
static void trackmouse(ModeInfo * mi)
{
    sballsstruct *sb = &sballs[MI_SCREEN(mi)];
    /* we keep static values (not per screen) for the mouse stuff: in general you have only one mouse :-> */
    static int max[2] = { 0, 0 };
    static int min[2] = { 0x7fffffff, 0x7fffffff }, center[2];
    Window r, c;
    int rx, ry, cx, cy;
    unsigned int m;

    (void) XQueryPointer(MI_DISPLAY(mi), MI_WINDOW(mi),
			 &r, &c, &rx, &ry, &cx, &cy, &m);

    if (max[0] < cx)
	max[0] = cx;
    if (min[0] > cx)
	min[0] = cx;
    center[0] = (max[0] + min[0]) / 2;

    if (max[1] < cy)
	max[1] = cy;
    if (min[1] > cy)
	min[1] = cy;
    center[1] = (max[1] + min[1]) / 2;

    if (fabs(center[0] - (float) cx) > 0.1 * (max[0] - min[0]))
	sb->rot[0] -= ((center[0] - (float) cx) / (max[0] - min[0]) * 180.0f) / 200.0f;
    if (fabs(center[1] - (float) cy) > 0.1 * (max[1] - min[1]))
        sb->rot[1] -= ((center[1] - (float) cy) / (max[1] - min[1]) * 180.0f) / 200.0f;;
    clamp(sb->rot);

    /* oops: can't get those buttons */
    if (m & Button4Mask)
        sb->speed++;
    if (m & Button5Mask)
        sb->speed--;

}

/* initialise textures */
static void inittextures(ModeInfo * mi)
{
    sballsstruct *sb = &sballs[MI_SCREEN(mi)];
    if (do_texture) {

	glGenTextures(1, &sb->backid);
#ifdef HAVE_GLBINDTEXTURE
	glBindTexture(GL_TEXTURE_2D, sb->backid);
#endif /* HAVE_GLBINDTEXTURE */

        sb->btexture = xpm_to_ximage(MI_DISPLAY(mi),
                                     MI_VISUAL(mi),
                                     MI_COLORMAP(mi),
                                     back_data);
	if (!(sb->btexture)) {
	    (void) fprintf(stderr, "Error reading the background texture.\n");
            glDeleteTextures(1, &sb->backid);
            do_texture = False;
            sb->faceid = 0;       /* default textures */
            sb->backid = 0;
	    return;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		     sb->btexture->width, sb->btexture->height, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, sb->btexture->data);

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
                                     face_data);
	if (!(sb->ftexture)) {
	    (void) fprintf(stderr, "Error reading the face texture.\n");
            glDeleteTextures(1, &sb->faceid);
	    sb->faceid = 0;
	    return;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		     sb->ftexture->width, sb->ftexture->height, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, sb->ftexture->data);

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

}

static void drawSphere(ModeInfo * mi,int sphere_num)
{
  sballsstruct *sb = &sballs[MI_SCREEN(mi)];
  float x = polygons[object].v[sphere_num][0];
  float y = polygons[object].v[sphere_num][1];
  float z = polygons[object].v[sphere_num][2];
  int numMajor = 10;
  int numMinor = 10;
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

static void Reshape(ModeInfo * mi)
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

    if (do_trackmouse && !MI_IS_ICONIC(mi))
	trackmouse(mi);

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

    /* rotate the mouse */
    glRotatef(sb->rot[0], 1.0f, 0.0f, 0.0f);
    glRotatef(sb->rot[1], 0.0f, 1.0f, 0.0f);
    glRotatef(sb->rot[2], 0.0f, 0.0f, 1.0f);

    /* rotate the balls */
    glRotatef(sb->rotm[0], 1.0f, 0.0f, 0.0f);
    glRotatef(sb->rotm[1], 0.0f, 1.0f, 0.0f);
    glRotatef(sb->rotm[2], 0.0f, 0.0f, 1.0f);

    sb->rotm[0] += sb->speed;
    sb->rotm[1] += -(sb->speed);
    sb->rotm[2] += 0;

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
    vinit(sb->rot   ,0.0f, 0.0f, 0.0f);
    vinit(sb->rotm  ,0.0f, 0.0f, 0.0f);
    sb->speed = MI_CYCLES(mi);

    /* initialise object number */
    if (object == 0)
	object = NRAND(MAX_OBJ);
    if ((object == 0) || (object > MAX_OBJ))
	object = DEF_OBJECT_INDX;
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
		       MI_CYCLES(mi),
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

void init_sballs(ModeInfo * mi)
{
    sballsstruct *sb;

    if (sballs == NULL) {
	if ((sballs = (sballsstruct *) calloc(MI_NUM_SCREENS(mi),
					  sizeof(sballsstruct))) == NULL)
	    return;
    }
    sb = &sballs[MI_SCREEN(mi)];

    if ((sb->glx_context = init_GL(mi)) != NULL) {

	Reshape(mi);
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
void draw_sballs(ModeInfo * mi)
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
    Reshape(mi);

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

void release_sballs(ModeInfo * mi)
{
    int screen;

    if (sballs != NULL) {
	for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
	    sballsstruct *sb = &sballs[screen];
            if (mode_font != None)
	    {
        	unsigned int first, last;
        	first = mode_font->min_char_or_byte2;
        	last  = mode_font->max_char_or_byte2;
		glDeleteLists(sb->fontbase,last - first + 1);
	    }
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
    if (mode_font != None)
    {
	/* only free-ed when there is no more screens used */
	XFreeFont(MI_DISPLAY(mi), mode_font);
	mode_font = None;
    }
    FreeAllGL(mi);
}

void change_sballs(ModeInfo * mi)
{
    sballsstruct *sb;

    if (sballs == NULL)
	    return;
    sb = &sballs[MI_SCREEN(mi)];

    if (!sb->glx_context)
	return;

    /* initialise object number */
    if (object == 0)
	object = NRAND(MAX_OBJ);
    if ((object == 0) || (object > MAX_OBJ))
	object = DEF_OBJECT_INDX;
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
		       MI_CYCLES(mi),
		       do_texture ? "on" : "off"
			);
    }
    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(sb->glx_context));

}
#endif /* MODE_sballs */
