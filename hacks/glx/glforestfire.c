/* -*- Mode: C; tab-width: 4 -*- */
/* fire --- 3D fire or rain landscape */

#if 0
static const char sccsid[] = "@(#)fire.c	5.02 2001/09/26 xlockmore";
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
 * The original code for this mode was written by David Bucciarelli 
 * (tech.hmw@plus.it) and could be found in the demo package 
 * of Mesa (Mesa-3.2/3Dfx/demos/). This mode is the result of the merge of
 * two of the David's demos (fire and rain).
 *
 * Eric Lassauge  (October-10-2000) <lassauge@users.sourceforge.net>
 * 				    http://lassauge.free.fr/linux.html
 *
 * REVISION HISTORY:
 *
 * E.Lassauge - 26-Sep-2001:
 *      - add wander option and code 
 *      - cleanups for xscreensaver
 *
 * E.Lassauge - 09-Mar-2001:
 *      - get rid of my framerate options to use showfps
 *
 * E.Lassauge - 12-Jan-2001:
 *      - add rain particules, selected if count=0 (no fire means rain !)
 *
 * E.Lassauge - 28-Nov-2000:
 *      - modified release part to add freeing of GL objects
 *
 * E.Lassauge - 14-Nov-2000:
 *      - use new common xpm_to_ximage function
 *
 * E.Lassauge - 25-Oct-2000:
 *	- add the trees (with a new resource '-trees')
 *      - corrected handling of color (textured vs untextured)
 *      - corrected handling of endiannes for the xpm files
 *      - inverted ground pixmap file
 *      - use malloc-ed tree array
 *
 * TSchmidt - 23-Oct-2000:
 *	- added size option like used in sproingies mode
 *
 * E.Lassauge - 13-Oct-2000:
 *	- when trackmouse and window is iconified (login screen): stop tracking
 *	- add pure GLX handling of framerate display (erased GLUT stuff)
 *	- made count a per screen variable and update it only if framemode
 *	- changes for no_texture an wireframe modes
 *	- change no_texture color for the ground
 *	- add freeing of texture image
 *	- misc comments and little tweakings
 *
 * TODO:
 *      - perhaps use a user supplied xpm for ground image (or a whatever image
 *        file using ImageMagick ?)
 *	- random number of trees ? change trees at change_fire ?
 *	- fix wireframe mode: it's too CPU intensive.
 *	- look how we can get the Wheel events (Button4&5).
 */


#ifdef STANDALONE	/* xscreensaver mode */
#define DEFAULTS "*delay:     10000 \n" \
		"*count: 	800 \n" \
		"*size:           0 \n" \
		"*showFPS:    False \n" \
		"*wireframe:  False \n"	\

# define refresh_fire 0
#define MODE_fire
#include "xlockmore.h"		/* from the xscreensaver distribution */
#include "gltrackball.h"
#else				/* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "visgl.h"
#endif				/* !STANDALONE */

#ifdef MODE_fire

#define MINSIZE 32

#if defined( USE_XPM ) || defined( USE_XPMINC ) || defined(STANDALONE)
/* USE_XPM & USE_XPMINC in xlock mode ; HAVE_XPM in xscreensaver mode */
#include "xpm-ximage.h"
#define I_HAVE_XPM

#ifdef STANDALONE
#include "../images/ground.xpm"
#include "../images/tree.xpm"
#else /* !STANDALONE */
#include "pixmaps/ground.xpm"
#include "pixmaps/tree.xpm"
#endif /* !STANDALONE */
#endif /* HAVE_XPM */

/* vector utility macros */
#define vinit(a,i,j,k) {\
  (a)[0]=i;\
  (a)[1]=j;\
  (a)[2]=k;\
}

#define vinit4(a,i,j,k,w) {\
  (a)[0]=i;\
  (a)[1]=j;\
  (a)[2]=k;\
  (a)[3]=w;\
}

#define vadds(a,dt,b) {\
  (a)[0]+=(dt)*(b)[0];\
  (a)[1]+=(dt)*(b)[1];\
  (a)[2]+=(dt)*(b)[2];\
}

#define vequ(a,b) {\
  (a)[0]=(b)[0];\
  (a)[1]=(b)[1];\
  (a)[2]=(b)[2];\
}

#define vinter(a,dt,b,c) {\
  (a)[0]=(dt)*(b)[0]+(1.0-dt)*(c)[0];\
  (a)[1]=(dt)*(b)[1]+(1.0-dt)*(c)[1];\
  (a)[2]=(dt)*(b)[2]+(1.0-dt)*(c)[2];\
}

#define clamp(a)        ((a) < 0.0 ? 0.0 : ((a) < 1.0 ? (a) : 1.0))

#define vclamp(v) {\
  (v)[0]=clamp((v)[0]);\
  (v)[1]=clamp((v)[1]);\
  (v)[2]=clamp((v)[2]);\
}

/* Manage option vars */
#define DEF_TEXTURE	"True"
#define DEF_FOG		"False"
#define DEF_SHADOWS	"True"
#define DEF_FRAMERATE	"False"
#define DEF_WANDER	"True"
#define DEF_TREES	"5"
#define MAX_TREES	20
static Bool do_texture;
static Bool do_fog;
static Bool do_shadows;
static Bool do_wander;
static int num_trees;
static XFontStruct *mode_font = None;

static XrmOptionDescRec opts[] = {
    {"-texture", ".fire.texture", XrmoptionNoArg, "on"},
    {"+texture", ".fire.texture", XrmoptionNoArg, "off"},
    {"-fog", ".fire.fog", XrmoptionNoArg, "on"},
    {"+fog", ".fire.fog", XrmoptionNoArg, "off"},
    {"-shadows", ".fire.shadows", XrmoptionNoArg, "on"},
    {"+shadows", ".fire.shadows", XrmoptionNoArg, "off"},
    {"-wander", ".fire.wander", XrmoptionNoArg, "on"},
    {"+wander", ".fire.wander", XrmoptionNoArg, "off"},
    {"-trees", ".fire.trees", XrmoptionSepArg, 0},
    {"-rain", ".fire.count", XrmoptionNoArg, "0"},

};

static argtype vars[] = {
    {&do_texture,    "texture",    "Texture",    DEF_TEXTURE,    t_Bool},
    {&do_fog,        "fog",        "Fog",        DEF_FOG,        t_Bool},
    {&do_shadows,    "shadows",    "Shadows",    DEF_SHADOWS,    t_Bool},
    {&do_wander,     "wander",     "Wander",     DEF_WANDER,     t_Bool},
    {&num_trees,     "trees",      "Trees",      DEF_TREES,      t_Int},
};

static OptionStruct desc[] = {
    {"-/+texture", "turn on/off texturing"},
    {"-/+fog", "turn on/off fog"},
    {"-/+shadows", "turn on/off shadows"},
    {"-/+wander", "turn on/off wandering"},
    {"-trees num", "number of trees (0 disables)"},
};

ENTRYPOINT ModeSpecOpt fire_opts =
 { sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc };

#ifdef USE_MODULES
ModStruct fire_description =
    { "fire", "init_fire", "draw_fire", "release_fire",
    "draw_fire", "change_fire", (char *) NULL, &fire_opts,
    10000, 800, 1, 400, 64, 1.0, "",
    "Shows a 3D fire-like image", 0, NULL
};
#endif /* USE_MODULES */

/* misc defines */
#define TREEINR		2.5	/* tree min distance */
#define TREEOUTR	8.0	/* tree max distance */
#define FRAME 		50	/* frame count interval */
#define DIMP 		20.0	/* dimension of ground */
#define DIMTP 		16.0	/* dimension of ground texture */

#define RIDCOL 		0.4	/* factor for color blending */

#define AGRAV 		-9.8	/* gravity */

#define NUMPART		7500	/* rain particles */

/* fire particle struct */
typedef struct {
    int age;
    float p[3][3];
    float v[3];
    float c[3][4];
} part;

/* rain particle struct */
typedef struct {
    float age;
    float acc[3];
    float vel[3];
    float pos[3];
    float partLength;
    float oldpos[3];
} rain;

/* colors */
static const GLfloat black[3]    = { 0.0, 0.0, 0.0 }; /* shadow color */
static const GLfloat partcol1[3] = { 1.0, 0.2, 0.0 }; /* initial color: red-ish */
static const GLfloat partcol2[3] = { 1.0, 1.0, 0.0 }; /* blending color: yellow-ish */
static const GLfloat fogcolor[4] = { 0.9, 0.9, 1.0, 1.0 };

/* ground */
static const float q[4][3] = {
    {-DIMP, 0.0, -DIMP},
    {DIMP, 0.0, -DIMP},
    {DIMP, 0.0, DIMP},
    {-DIMP, 0.0, DIMP}
};

/* ground texture */
static const float qt[4][2] = {
    {-DIMTP, -DIMTP},
    {DIMTP, -DIMTP},
    {DIMTP, DIMTP},
    {-DIMTP, DIMTP}
};

/* default values for observer */
static const float DEF_OBS[3] = { 2.0f, 1.0f, 0.0f };
#define DEV_V		0.0
#define DEF_ALPHA	-90.0
#define DEF_BETA	90.0

/* tree struct */
typedef struct {
    float x,y,z;
} treestruct;

/* the mode struct, contains all per screen variables */
typedef struct {
    GLint WIDTH, HEIGHT;	/* display dimensions */
    GLXContext *glx_context;

    int np;			/* number of fire particles : set it through 'count' resource */
    float eject_r;		/* emission radius */
    float dt, maxage, eject_vy, eject_vl;
    float ridtri;		/* fire particle size */
    Bool shadows;		/* misc booleans: set them through specific resources */
    Bool fog;

    part *p;			/* fire particles array */
    rain *r;			/* rain particles array */

    XImage *gtexture;		/* ground texture image bits */
    XImage *ttexture;		/* tree texture image bits */
    GLuint groundid;		/* ground texture id: GL world */
    GLuint treeid;		/* tree texture id: GL world */
    GLuint fontbase;		/* fontbase id: GL world */

    int   num_trees;		/* number of trees: set it through 'trees' resource */
    treestruct *treepos;	/* trees positions: float treepos[num_trees][3] */

    float min[3];		/* raining area */
    float max[3];

    float obs[3];		/* observer coordinates */
    float dir[3];		/* view direction */
    float v;			/* observer velocity */
    float alpha;		/* observer angles */
    float beta;

    trackball_state *trackball;
    Bool button_down_p;
    int frame;

} firestruct;

/* array of firestruct indexed by screen number */
static firestruct *fire = (firestruct *) NULL;

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Misc funcs.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

/* utility function for the rain particles */
static float gettimerain(void)
{
#if 0
  /* Oh yeah, *that's* portable!  WTF. */
  /* 
   * I really thought clock() was standard ... EL
   * I found this on the net:
   * The clock() function conforms to ISO/IEC 9899:1990 (``ISO C89'') 
   * */

  static clock_t told= (clock_t)0;
  clock_t tnew,ris;

  tnew=clock();

  ris=tnew-told;

  told=tnew;

  return (0.0125 + ris/(float)CLOCKS_PER_SEC);
#else
  return 0.0150;
#endif
}

/* my RAND */
static float vrnd(void)
{
    return ((float) LRAND() / (float) MAXRAND);
}

/* initialise new fire particle */
static void setnewpart(firestruct * fs, part * p)
{
    float a, vi[3];
    const float *c;

    p->age = 0;

    a = vrnd() * M_PI * 2.0;

    vinit(vi, sin(a) * fs->eject_r * vrnd(), 0.15, cos(a) * fs->eject_r * vrnd());
    vinit(p->p[0], vi[0] + vrnd() * fs->ridtri, vi[1] + vrnd() * fs->ridtri, vi[2] + vrnd() * fs->ridtri);
    vinit(p->p[1], vi[0] + vrnd() * fs->ridtri, vi[1] + vrnd() * fs->ridtri, vi[2] + vrnd() * fs->ridtri);
    vinit(p->p[2], vi[0] + vrnd() * fs->ridtri, vi[1] + vrnd() * fs->ridtri, vi[2] + vrnd() * fs->ridtri);

    vinit(p->v, vi[0] * fs->eject_vl / (fs->eject_r / 2),
	  vrnd() * fs->eject_vy + fs->eject_vy / 2,
	  vi[2] * fs->eject_vl / (fs->eject_r / 2));

    c = partcol1;

    vinit4(p->c[0], c[0] * ((1.0 - RIDCOL) + vrnd() * RIDCOL),
	   c[1] * ((1.0 - RIDCOL) + vrnd() * RIDCOL),
	   c[2] * ((1.0 - RIDCOL) + vrnd() * RIDCOL), 1.0);
    vinit4(p->c[1], c[0] * ((1.0 - RIDCOL) + vrnd() * RIDCOL),
	   c[1] * ((1.0 - RIDCOL) + vrnd() * RIDCOL),
	   c[2] * ((1.0 - RIDCOL) + vrnd() * RIDCOL), 1.0);
    vinit4(p->c[2], c[0] * ((1.0 - RIDCOL) + vrnd() * RIDCOL),
	   c[1] * ((1.0 - RIDCOL) + vrnd() * RIDCOL),
	   c[2] * ((1.0 - RIDCOL) + vrnd() * RIDCOL), 1.0);
}

/* initialise new rain particle */
static void setnewrain(firestruct * fs, rain * r)
{
    r->age=0.0f;

    vinit(r->acc,0.0f,-0.98f,0.0f);
    vinit(r->vel,0.0f,0.0f,0.0f);
    
    r->partLength=0.2f;

    vinit(r->oldpos,fs->min[0]+(fs->max[0]-fs->min[0])*vrnd(),
                    fs->max[1]+0.2f*fs->max[1]*vrnd(),
                    fs->min[2]+(fs->max[2]-fs->min[2])*vrnd());
    vequ(r->pos,r->oldpos);
    vadds(r->oldpos,-(r->partLength),r->vel);

    r->pos[1]=(fs->max[1]-fs->min[1])*vrnd()+fs->min[1];
    r->oldpos[1]=r->pos[1]-r->partLength*r->vel[1];
}

/* set fire particle values */
static void setpart(firestruct * fs, part * p)
{
    float fact;

    if (p->p[0][1] < 0.1) {
	setnewpart(fs, p);
	return;
    }

    p->v[1] += AGRAV * fs->dt;

    vadds(p->p[0], fs->dt, p->v);
    vadds(p->p[1], fs->dt, p->v);
    vadds(p->p[2], fs->dt, p->v);

    p->age++;

    if ((p->age) > fs->maxage) {
	vequ(p->c[0], partcol2);
	vequ(p->c[1], partcol2);
	vequ(p->c[2], partcol2);
    } else {
	fact = 1.0 / fs->maxage;
	vadds(p->c[0], fact, partcol2);
	vclamp(p->c[0]);
	p->c[0][3] = fact * (fs->maxage - p->age);

	vadds(p->c[1], fact, partcol2);
	vclamp(p->c[1]);
	p->c[1][3] = fact * (fs->maxage - p->age);

	vadds(p->c[2], fact, partcol2);
	vclamp(p->c[2]);
	p->c[2][3] = fact * (fs->maxage - p->age);
    }
}

/* set rain particle values */
static void setpartrain(firestruct * fs, rain * r, float dt)
{
    r->age += dt;

    vadds(r->vel,dt,r->acc);
    vadds(r->pos,dt,r->vel);

    if(r->pos[0]<fs->min[0])
	r->pos[0]=fs->max[0]-(fs->min[0]-r->pos[0]);
    if(r->pos[2]<fs->min[2])
	r->pos[2]=fs->max[2]-(fs->min[2]-r->pos[2]);

    if(r->pos[0]>fs->max[0])
	r->pos[0]=fs->min[0]+(r->pos[0]-fs->max[0]);
    if(r->pos[2]>fs->max[2])
	r->pos[2]=fs->min[2]+(r->pos[2]-fs->max[2]);

    vequ(r->oldpos,r->pos);
    vadds(r->oldpos,-(r->partLength),r->vel);
    if(r->pos[1]<fs->min[1])
    	setnewrain(fs, r);
}

/* draw a tree */
static int drawtree(float x, float y, float z)
{
    int polys = 0;
    glBegin(GL_QUADS);
    glTexCoord2f(0.0,0.0);
    glVertex3f(x-1.5,y+0.0,z);

    glTexCoord2f(1.0,0.0);
    glVertex3f(x+1.5,y+0.0,z);

    glTexCoord2f(1.0,1.0);
    glVertex3f(x+1.5,y+3.0,z);

    glTexCoord2f(0.0,1.0);
    glVertex3f(x-1.5,y+3.0,z);
    polys++;


    glTexCoord2f(0.0,0.0);
    glVertex3f(x,y+0.0,z-1.5);

    glTexCoord2f(1.0,0.0);
    glVertex3f(x,y+0.0,z+1.5);

    glTexCoord2f(1.0,1.0);
    glVertex3f(x,y+3.0,z+1.5);

    glTexCoord2f(0.0,1.0);
    glVertex3f(x,y+3.0,z-1.5);
    polys++;

    glEnd();

    return polys;
}

/* calculate observer position : modified only if trackmouse is used */
static void calcposobs(firestruct * fs)
{
    fs->dir[0] = sin(fs->alpha * M_PI / 180.0);
    fs->dir[2] =
	cos(fs->alpha * M_PI / 180.0) * sin(fs->beta * M_PI / 180.0);
    fs->dir[1] = cos(fs->beta * M_PI / 180.0);

    fs->obs[0] += fs->v * fs->dir[0];
    fs->obs[1] += fs->v * fs->dir[1];
    fs->obs[2] += fs->v * fs->dir[2];

    if (!fs->np)
    {
    	vinit(fs->min,fs->obs[0]-7.0f,-0.2f,fs->obs[2]-7.0f);
    	vinit(fs->max,fs->obs[0]+7.0f,8.0f,fs->obs[2]+7.0f);
    }
}


/* initialise textures */
static void inittextures(ModeInfo * mi)
{
    firestruct *fs = &fire[MI_SCREEN(mi)];

#if defined( I_HAVE_XPM )
    if (do_texture) {

	glGenTextures(1, &fs->groundid);
#ifdef HAVE_GLBINDTEXTURE
	glBindTexture(GL_TEXTURE_2D, fs->groundid);
#endif /* HAVE_GLBINDTEXTURE */

        if ((fs->gtexture = xpm_to_ximage(MI_DISPLAY(mi), MI_VISUAL(mi),
			 MI_COLORMAP(mi), ground)) == None) {
	    (void) fprintf(stderr, "Error reading the ground texture.\n");
	    glDeleteTextures(1, &fs->groundid);
            do_texture = False;
	    fs->groundid = 0;
	    fs->treeid   = 0;
	    return;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    clear_gl_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 fs->gtexture->width, fs->gtexture->height, 0,
                 GL_RGBA,
                 /* GL_UNSIGNED_BYTE, */
                 GL_UNSIGNED_INT_8_8_8_8_REV,
                 fs->gtexture->data);
    check_gl_error("texture");

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

        if (fs->num_trees)
	{
	    glGenTextures(1, &fs->treeid);
#ifdef HAVE_GLBINDTEXTURE
	    glBindTexture(GL_TEXTURE_2D,fs->treeid);
#endif /* HAVE_GLBINDTEXTURE */
            if ((fs->ttexture = xpm_to_ximage(MI_DISPLAY(mi), MI_VISUAL(mi),
			 MI_COLORMAP(mi), tree)) == None) {
	      (void)fprintf(stderr,"Error reading tree texture.\n");
	      glDeleteTextures(1, &fs->treeid);
	      fs->treeid    = 0;
              fs->num_trees = 0; 
	      return;
	    }

        clear_gl_error();
	    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     fs->ttexture->width, fs->ttexture->height, 0,
                     GL_RGBA,
                     /* GL_UNSIGNED_BYTE, */
                     GL_UNSIGNED_INT_8_8_8_8_REV,
                     fs->ttexture->data);
        check_gl_error("texture");

	    glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	    glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

	    glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	    glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

	    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
	}
    }
    else
    {
	fs->groundid = 0;	/* default textures */
	fs->treeid   = 0;
    }
#else /* !I_HAVE_XPM */
  do_texture = False;
  fs->groundid = 0;       /* default textures */
  fs->treeid = 0;
#endif /* !I_HAVE_XPM */
}

/* init tree array and positions */
static Bool inittree(ModeInfo * mi)
{
    firestruct *fs = &fire[MI_SCREEN(mi)];
    int i;
    float dist;

    /* allocate treepos array */
    if ((fs->treepos = (treestruct *) malloc(fs->num_trees *
					sizeof(treestruct))) == NULL) {
		return False;
    }
    /* initialise positions */
    for(i=0;i<fs->num_trees;i++)
    	do {
      	    fs->treepos[i].x =vrnd()*TREEOUTR*2.0-TREEOUTR;
      	    fs->treepos[i].y =0.0;
      	    fs->treepos[i].z =vrnd()*TREEOUTR*2.0-TREEOUTR;
      	    dist=sqrt(fs->treepos[i].x *fs->treepos[i].x +fs->treepos[i].z *fs->treepos[i].z );
        } while((dist<TREEINR) || (dist>TREEOUTR));
	return True;
}

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    GL funcs.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void reshape_fire(ModeInfo * mi, int width, int height)
{

    firestruct *fs = &fire[MI_SCREEN(mi)];
    int size = MI_SIZE(mi);

    /* Viewport is specified size if size >= MINSIZE && size < screensize */
    if (size <= 1) {
        fs->WIDTH = MI_WIDTH(mi);
        fs->HEIGHT = MI_HEIGHT(mi);
    } else if (size < MINSIZE) {
        fs->WIDTH = MINSIZE;
        fs->HEIGHT = MINSIZE;
    } else {
        fs->WIDTH = (size > MI_WIDTH(mi)) ? MI_WIDTH(mi) : size;
        fs->HEIGHT = (size > MI_HEIGHT(mi)) ? MI_HEIGHT(mi) : size;
    }
    glViewport((MI_WIDTH(mi) - fs->WIDTH) / 2, (MI_HEIGHT(mi) - fs->HEIGHT) / 2, fs->WIDTH, fs->HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70.0, fs->WIDTH / (float) fs->HEIGHT, 0.1, 30.0);

    glMatrixMode(GL_MODELVIEW);

}

static void DrawFire(ModeInfo * mi)
{
    int j;
    firestruct *fs = &fire[MI_SCREEN(mi)];
    Bool wire = MI_IS_WIREFRAME(mi);

    mi->polygon_count = 0;

    if (do_wander && !fs->button_down_p)
    {
	GLfloat x, y, z;

#       define SINOID(SCALE,SIZE) \
        ((((1 + sin((fs->frame * (SCALE)) / 2 * M_PI)) / 2.0) * (SIZE)) - (SIZE)/2)

        x = SINOID(0.031, 0.85);
        y = SINOID(0.017, 0.25);
        z = SINOID(0.023, 0.85);
        fs->frame++;
        fs->obs[0] = x + DEF_OBS[0];
        fs->obs[1] = y + DEF_OBS[1];
        fs->obs[2] = z + DEF_OBS[2];
        fs->dir[1] = y;
        fs->dir[2] = z;
    }

    glEnable(GL_DEPTH_TEST);

    if (fs->fog)
	glEnable(GL_FOG);
    else
	glDisable(GL_FOG);

    glDepthMask(GL_TRUE);
    glClearColor(0.5, 0.5, 0.8, 1.0);	/* sky in the distance */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();

    calcposobs(fs);

    gltrackball_rotate (fs->trackball);

    gluLookAt(fs->obs[0], fs->obs[1], fs->obs[2],
              fs->obs[0] + fs->dir[0], 
              fs->obs[1] + fs->dir[1],
              fs->obs[2] + fs->dir[2],
              0.0, 1.0, 0.0);

    glEnable(GL_TEXTURE_2D);

    /* draw ground using the computed texture */
    if (do_texture) {
	glColor4f(1.0,1.0,1.0,1.0);	/* white to get texture in it's true color */
#ifdef HAVE_GLBINDTEXTURE
	glBindTexture(GL_TEXTURE_2D, fs->groundid);
#endif /* HAVE_GLBINDTEXTURE */
    }
    else
        glColor4f(0.54, 0.27, 0.07, 1.0);	/* untextured ground color */
    glBegin(GL_QUADS);
    glTexCoord2fv(qt[0]);
    glVertex3fv(q[0]);
    glTexCoord2fv(qt[1]);
    glVertex3fv(q[1]);
    glTexCoord2fv(qt[2]);
    glVertex3fv(q[2]);
    glTexCoord2fv(qt[3]);
    glVertex3fv(q[3]);
    mi->polygon_count++;
    glEnd();

    glAlphaFunc(GL_GEQUAL, 0.9);
    if (fs->num_trees)
    {
	/* here do_texture IS True - and color used is white */
	glEnable(GL_ALPHA_TEST);
#ifdef HAVE_GLBINDTEXTURE
	glBindTexture(GL_TEXTURE_2D,fs->treeid);
#endif /* HAVE_GLBINDTEXTURE */
	for(j=0;j<fs->num_trees;j++)
      mi->polygon_count += drawtree(fs->treepos[j].x ,fs->treepos[j].y ,fs->treepos[j].z );
    	glDisable(GL_ALPHA_TEST);
    }
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);

    if (fs->shadows) {
	/* draw shadows with black color */
	glBegin(wire ? GL_LINE_STRIP : GL_TRIANGLES);
	for (j = 0; j < fs->np; j++) {
	    glColor4f(black[0], black[1], black[2], fs->p[j].c[0][3]);
	    glVertex3f(fs->p[j].p[0][0], 0.1, fs->p[j].p[0][2]);

	    glColor4f(black[0], black[1], black[2], fs->p[j].c[1][3]);
	    glVertex3f(fs->p[j].p[1][0], 0.1, fs->p[j].p[1][2]);

	    glColor4f(black[0], black[1], black[2], fs->p[j].c[2][3]);
	    glVertex3f(fs->p[j].p[2][0], 0.1, fs->p[j].p[2][2]);
        mi->polygon_count++;
	}
	glEnd();
    }

    glBegin(wire ? GL_LINE_STRIP : GL_TRIANGLES);
    for (j = 0; j < fs->np; j++) {
	/* draw particles: colors are computed in setpart */
	glColor4fv(fs->p[j].c[0]);
	glVertex3fv(fs->p[j].p[0]);

	glColor4fv(fs->p[j].c[1]);
	glVertex3fv(fs->p[j].p[1]);

	glColor4fv(fs->p[j].c[2]);
	glVertex3fv(fs->p[j].p[2]);
    mi->polygon_count++;

	setpart(fs, &fs->p[j]);
    }
    glEnd();

    /* draw rain particles if no fire particles */
    if (!fs->np)
    {
        float timeused = gettimerain();
        glDisable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glBegin(GL_LINES);
	for (j = 0; j < NUMPART; j++) {
	    glColor4f(0.7f,0.95f,1.0f,0.0f);
	    glVertex3fv(fs->r[j].oldpos);
	    glColor4f(0.3f,0.7f,1.0f,1.0f);
	    glVertex3fv(fs->r[j].pos);
	    setpartrain(fs, &fs->r[j],timeused);
        mi->polygon_count++;
	}
	glEnd();
	glShadeModel(GL_FLAT);
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);

    /* manage framerate display */
    if (MI_IS_FPS(mi)) do_fps (mi);
    glPopMatrix();
}


static Bool Init(ModeInfo * mi)
{
    int i;
    firestruct *fs = &fire[MI_SCREEN(mi)];

    /* default settings */
    fs->eject_r = 0.1 + NRAND(10) * 0.03;
    fs->dt = 0.015;
    fs->eject_vy = 4;
    fs->eject_vl = 1;
    fs->ridtri = 0.1 + NRAND(10) * 0.005;
    fs->maxage = 1.0 / fs->dt;
    vinit(fs->obs, DEF_OBS[0], DEF_OBS[1], DEF_OBS[2]);
    fs->v = 0.0;
    fs->alpha = DEF_ALPHA;
    fs->beta = DEF_BETA;

    /* initialise texture stuff */
    if (do_texture)
    	inittextures(mi);
    else
    {
	fs->ttexture = (XImage*) NULL;
	fs->gtexture = (XImage*) NULL;
    }

    if (MI_IS_DEBUG(mi)) {
	(void) fprintf(stderr,
		       "%s:\n\tnum_part=%d\n\ttrees=%d\n\tfog=%s\n\tshadows=%s\n\teject_r=%.3f\n\tridtri=%.3f\n",
		       MI_NAME(mi),
		       fs->np,
		       fs->num_trees,
		       fs->fog ? "on" : "off",
		       fs->shadows ? "on" : "off",
		       fs->eject_r, fs->ridtri);
    }

    /* initialise particles and trees */
    for (i = 0; i < fs->np; i++) {
	setnewpart(fs, &(fs->p[i]));
    }

    if (fs->num_trees)
	if (!inittree(mi)) {
		return False;
	}

    /* if no fire particles then initialise rain particles */
    if (!fs->np)
    {
	vinit(fs->min,-7.0f,-0.2f,-7.0f);
  	vinit(fs->max,7.0f,8.0f,7.0f);
    	for (i = 0; i < NUMPART; i++) {
	    setnewrain(fs, &(fs->r[i]));
    	}
    }
    
    return True;
}

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Xlock hooks.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */


static void
free_fire(firestruct *fs)
{
	if (mode_font != None && fs->fontbase != None) {
		glDeleteLists(fs->fontbase, mode_font->max_char_or_byte2 -
			mode_font->min_char_or_byte2 + 1);
		fs->fontbase = None;
	}

	if (fs->p != NULL) {
		(void) free((void *) fs->p);
		fs->p = (part *) NULL;
	}
	if (fs->r != NULL) {
		(void) free((void *) fs->r);
		fs->r = (rain *) NULL;
	}
	if (fs->treepos != NULL) {
		(void) free((void *) fs->treepos);
		fs->treepos = (treestruct *) NULL;
	}
	if (fs->ttexture != None) {
		glDeleteTextures(1, &fs->treeid);
		XDestroyImage(fs->ttexture);
		fs->ttexture = None;
	}
	if (fs->gtexture != None) {
		glDeleteTextures(1, &fs->groundid);
		XDestroyImage(fs->gtexture);
		fs->gtexture = None;
	}
}

/*
 *-----------------------------------------------------------------------------
 *    Initialize fire.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

ENTRYPOINT void
init_fire(ModeInfo * mi)
{
    firestruct *fs;

    /* allocate the main fire table if needed */
    if (fire == NULL) {
	if ((fire = (firestruct *) calloc(MI_NUM_SCREENS(mi),
					  sizeof(firestruct))) == NULL)
	    return;
    }

    /* initialise the per screen fire structure */
    fs = &fire[MI_SCREEN(mi)];
    fs->np = MI_COUNT(mi);
    fs->fog = do_fog;
    fs->shadows = do_shadows;
    /* initialise fire particles if any */
    if ((fs->np)&&(fs->p == NULL)) {
	if ((fs->p = (part *) calloc(fs->np, sizeof(part))) == NULL) {
	    free_fire(fs);
	    return;
	}
    }
    else if (fs->r == NULL) {
        /* initialise rain particles if no fire particles */
	if ((fs->r = (rain *) calloc(NUMPART, sizeof(part))) == NULL) {
	    free_fire(fs);
	    return;
	}
    }

    /* check tree number */
    if (do_texture)
    	fs->num_trees = (num_trees<MAX_TREES)?num_trees:MAX_TREES;
    else
    	fs->num_trees = 0;

    fs->trackball = gltrackball_init (False);

    /* xlock GL stuff */
    if ((fs->glx_context = init_GL(mi)) != NULL) {

#ifndef STANDALONE
	Reshape(mi); /* xlock mode */
#else
	reshape_fire(mi,MI_WIDTH(mi),MI_HEIGHT(mi)); /* xscreensaver mode */
#endif
	glDrawBuffer(GL_BACK);
	if (!Init(mi)) {
		free_fire(fs);
		return;
	}
    } else {
	MI_CLEARWINDOW(mi);
    }
}

/*
 *-----------------------------------------------------------------------------
 *    Called by the mainline code periodically to update the display.
 *-----------------------------------------------------------------------------
 */
ENTRYPOINT void draw_fire(ModeInfo * mi)
{
    firestruct *fs = &fire[MI_SCREEN(mi)];

    Display *display = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);

    MI_IS_DRAWN(mi) = True;

    if (!fs->glx_context)
	return;

    glXMakeCurrent(display, window, *(fs->glx_context));

    glShadeModel(GL_FLAT);
    glEnable(GL_DEPTH_TEST);

    /* makes particles blend with background */
    if (!MI_IS_WIREFRAME(mi)||(!fs->np))
    {
    	glEnable(GL_BLEND);
    	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    /* fog stuff */
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP);
    glFogfv(GL_FOG_COLOR, fogcolor);
    glFogf(GL_FOG_DENSITY, 0.03);
    glHint(GL_FOG_HINT, GL_NICEST);

    glPushMatrix();
    glRotatef(current_device_rotation(), 0, 0, 1);
    DrawFire(mi);
    glPopMatrix();
#ifndef STANDALONE
    Reshape(mi); /* xlock mode */
#else
    reshape_fire(mi,MI_WIDTH(mi),MI_HEIGHT(mi)); /* xscreensaver mode */
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

ENTRYPOINT void release_fire(ModeInfo * mi)
{
    if (fire != NULL) {
    int screen;
	for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
		free_fire(&fire[screen]);
	(void) free((void *) fire);
	fire = (firestruct *) NULL;
    }
    if (mode_font != None)
    {
	/* only free-ed when there are no more screens used */
	XFreeFont(MI_DISPLAY(mi), mode_font);
	mode_font = None;
    }
    FreeAllGL(mi);
}

ENTRYPOINT Bool
fire_handle_event (ModeInfo *mi, XEvent *event)
{
  firestruct *fs = &fire[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, fs->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &fs->button_down_p))
    return True;

  return False;
}


#ifndef STANDALONE
ENTRYPOINT void change_fire(ModeInfo * mi)
{
    firestruct *fs = &fire[MI_SCREEN(mi)];

    if (!fs->glx_context)
	return;

    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(fs->glx_context));

    /* if available, randomly change some values */
    if (do_fog)
	fs->fog = LRAND() & 1;
    if (do_shadows)
	fs->shadows = LRAND() & 1;
    /* reset observer position */
    frame = 0;
    vinit(fs->obs, DEF_OBS[0], DEF_OBS[1], DEF_OBS[2]);
    fs->v = 0.0;
    /* particle randomisation */
    fs->eject_r = 0.1 + NRAND(10) * 0.03;
    fs->ridtri = 0.1 + NRAND(10) * 0.005;

    if (MI_IS_DEBUG(mi)) {
	(void) fprintf(stderr,
		       "%s:\n\tnum_part=%d\n\ttrees=%d\n\tfog=%s\n\tshadows=%s\n\teject_r=%.3f\n\tridtri=%.3f\n",
		       MI_NAME(mi),
		       fs->np,
		       fs->num_trees,
		       fs->fog ? "on" : "off",
		       fs->shadows ? "on" : "off",
		       fs->eject_r, fs->ridtri);
    }
}
#endif /* !STANDALONE */

XSCREENSAVER_MODULE_2 ("GLForestFire", glforestfire, fire)

#endif /* MODE_fire */
