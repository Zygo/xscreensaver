/* -*- Mode: C; tab-width: 4 -*- */
/* text3d --- Shows moving 3D texts */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)text3d.cc	5.02 2001/11/09 xlockmore";

#endif

/* Copyright (c) E. Lassauge, 1998. */

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
 * This module is based on a demo of the gltt graphics library
 * Copyright (C) 1998 Stephane Rehel.
 *
 * See the gltt Official Site at http://gltt.sourceforge.net/
 * May have better luck at http://lassauge.free.fr/xlock/
 *
 * My e-mail address changed to <lassauge AT users.sourceforge.net>
 * Web site at http://lassauge.free.fr/
 *
 * Eric Lassauge  (October-28-1999)
 *
 * Revision History:
 * 09-Nov-2001: Removed BLANK and various cleanups
 *              Added Wander animation
 * 09-Mar-2001: Removed an erroneous PushMatrix !!!
 * 01-Nov-2000: Allocation checks
 * 28-Oct-1999: fixes from Jouk "I play with every mode" Jansen.
 *               Option ttanimate added.
 * 02-Jun-1999: patches for initialization errors of GLTT library.
 *              Thanks to Jouk Jansen and Scott <mcmillan@cambridge.com>.
 *              text3d updates for fortunes thanks to Jouk Jansen
 *              <joukj@hrem.stm.tudelft.nl>
 *              Option no_split added.
 * 23-Aug-1998: add better handling of "faulty" fontfile and randomize
 *              fontfile if '-ttfont' value is a directory.
 *              Minor changes for AIX from Jouk Jansen
 *              (joukj@hrem.stm.tudelft.nl).
 *
 * TODO :
 *       Need more animation functions. Help welcome !!
 *       Light problem with some letters (don't know why they "reflect" more):
 *       is the problem in gltt or Mesa ???
 *       SPEED !!!!
 *       It may sigfault when compiled with -fschedule-insns2
 *         (i686-pc-linux-gnu)
 *
 */

#ifdef STANDALONE	/* xscreensaver mode: can't work ! */
#define MODE_text3d
#define PROGCLASS 	"Text3d"
#define HACK_INIT 	init_text3d
#define HACK_DRAW 	draw_text3d
#define HACK_RESHAPE 	reshape_text3d
#define text3d_opts 	xlockmore_opts
#define DEFAULTS 	"*delay: 	100000 \n" \
 			"*ncolors: 	    64 \n" \
 			"*font: 	       \n" \
 			"*text: 	       \n" \
 			"*filename: 	       \n" \
 			"*fortunefile: 	       \n" \
 			"*program: 	       \n"

extern "C"
{
#include "xlockmore.h"		/* from the xscreensaver distribution */
}
#else				/* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "visgl.h"
#ifdef HAS_MMOV
#undef error
#endif
#endif				/* !STANDALONE */
#include "iostuff.h"

#ifdef MODE_text3d

#include <gltt/FTEngine.h>
#include <gltt/FTFace.h>
#include <gltt/FTInstance.h>
#include <gltt/FTGlyph.h>
#include <gltt/FTFont.h>
#include <gltt/GLTTOutlineFont.h>
#include <gltt/GLTTFont.h>
#include <gltt/GLTTGlyphPolygonizer.h>
#include <gltt/GLTTGlyphTriangulator.h>

#include "text3d.h"
#include <GL/glu.h>

/* Yes, it's an ugly mix of 'C' and 'C++' functions */
extern "C" { void init_text3d(ModeInfo * mi); }
extern "C" { void draw_text3d(ModeInfo * mi); }
extern "C" { void change_text3d(ModeInfo * mi); }
extern "C" { void release_text3d(ModeInfo * mi); }
extern "C" { void refresh_text3d(ModeInfo * mi); }

/* arial.ttf is not supplied for legal reasons. */
/* NT and Windows 3.1 in c:\WINDOWS\SYSTEM\ARIAL.TTF */
/* Windows95 in c:\windows\fonts\arial.ttf */

#ifndef DEF_TTFONT
/* Directory of only *.ttf */
/* symbol.ttf and wingding.ttf should be excluded or it may core dump */
/* can be excluded if gltt is modified see README */
#define DEF_TTFONT "/usr/lib/X11/xlock/fonts/"
#endif

#define DEF_EXTRUSION  "25.0"
#define DEF_ROTAMPL  "1.0"
#define DEF_ROTFREQ  "0.001"
#define DEF_FONTSIZE  220
#define DEF_NOSPLIT	0
#define DEF_ANIMATE   "Default"
static float extrusion;
static float rampl;
static float rfreq;
static char *mode_font;
static int nosplit;
static char *animate;

/* Manage Option vars */

static XrmOptionDescRec opts[] =
{
    {(char *) "-ttfont", (char *) ".text3d.ttfont", XrmoptionSepArg, (caddr_t) NULL},
    {(char *) "-extrusion", (char *) ".text3d.extrusion", XrmoptionSepArg, (caddr_t) NULL},
    {(char *) "-rot_amplitude", (char *) ".text3d.rot_amplitude", XrmoptionSepArg, (caddr_t) NULL},
    {(char *) "-rot_frequency", (char *) ".text3d.rot_frequency", XrmoptionSepArg, (caddr_t) NULL},
    {(char *) "-no_split", (char *) ".text3d.no_split", XrmoptionNoArg, (caddr_t) "on"},
    {(char *) "+no_split", (char *) ".text3d.no_split", XrmoptionNoArg, (caddr_t) "off"},
    {(char *) "-ttanimate", (char *) ".text3d.ttanimate", XrmoptionSepArg, (caddr_t) NULL},
};

static argtype vars[] =
{
    {(void *) & mode_font, (char *) "ttfont", (char *) "TTFont", (char *) DEF_TTFONT, t_String},
    {(void *) & extrusion, (char *) "extrusion", (char *) "Extrusion", (char *) DEF_EXTRUSION, t_Float},
    {(void *) & rampl, (char *) "rot_amplitude", (char *) "RotationAmplitude", (char *) DEF_ROTAMPL, t_Float},
    {(void *) & rfreq, (char *) "rot_frequency", (char *) "RotationFrequency", (char *) DEF_ROTFREQ, t_Float},
    {(void *) & nosplit, (char *) "no_split", (char *) "NoSplit", (char *) DEF_NOSPLIT, t_Bool},
    {(void *) & animate, (char *) "ttanimate", (char *) "TTAnimate", (char *) DEF_ANIMATE, t_String},
};

static OptionStruct desc[] =
{
    {(char *) "-ttfont filename", (char *) "Text3d TrueType font file name"},
    {(char *) "-extrusion float", (char *) "Text3d extrusion length"},
    {(char *) "-rot_amplitude float", (char *) "Text3d rotation amplitude"},
    {(char *) "-rot_frequency float", (char *) "Text3d rotation frequency"},
    {(char *) "-/+no_split", (char *) "Text3d words splitting off/on"},
    {(char *) "-ttanimate anim_name", (char *) "Text3d animation function"},
};

ModeSpecOpt text3d_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct text3d_description =
{"text3d", "init_text3d", "draw_text3d", "release_text3d",
 "refresh_text3d", "change_text3d", (char *) NULL, &text3d_opts,
 100000, 10, 1, 1, 64, 1.0, "",
 "Shows 3D text", 0, NULL};
#endif

static text3dstruct *text3d = (text3dstruct *) NULL;

const double angle_speed = 2.5 / 180.0 * M_PI;

extern "C" {
typedef void (*t3dAnimProc) (text3dstruct * tp);
}

#ifdef __cplusplus
extern "C" {
#endif

static void t3d_anim_fullrandom(text3dstruct * tp);
static void t3d_anim_default(text3dstruct * tp);
static void t3d_anim_default2(text3dstruct * tp);
static void t3d_anim_none(text3dstruct * tp);
static void t3d_anim_crazy(text3dstruct * tp);
static void t3d_anim_updown(text3dstruct * tp);
static void t3d_anim_extrusion(text3dstruct * tp);
static void t3d_anim_rotatexy(text3dstruct * tp);
static void t3d_anim_rotateyz(text3dstruct * tp);
static void t3d_anim_frequency(text3dstruct * tp);
static void t3d_anim_amplitude(text3dstruct * tp);
static void t3d_anim_wander(text3dstruct * tp);
static char *fontfile = (char *) NULL;

#ifdef __cplusplus
}
#endif

static t3dAnimProc anim_array[] =
{
	t3d_anim_fullrandom,
	t3d_anim_default,
	t3d_anim_default2,
	t3d_anim_none,
	t3d_anim_crazy,
	t3d_anim_updown,
	t3d_anim_extrusion,
	t3d_anim_rotatexy,
	t3d_anim_rotateyz,
	t3d_anim_frequency,
	t3d_anim_amplitude,
	t3d_anim_wander,
};

static char * anim_names[] =
{
	(char *) "Random",
	(char *) "FullRandom",
	(char *) "Default",
	(char *) "Default2",
	(char *) "None",
	(char *) "Crazy",
	(char *) "UpDown",
	(char *) "Extrude",
	(char *) "RotateXY",
	(char *) "RotateYZ",
	(char *) "Frequency",	/* needs -rot_frequency /= 0.0 */
	(char *) "Amplitude",	/* and   -rot_amplitude /= 0.0 */
	(char *) "Wander",
	(char *) NULL
};

static int anims=sizeof anim_array / sizeof anim_array[0] ;

/*
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 *    Mode funcs.
 *-----------------------------------------------------------------------------
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 *    Utils.
 *-----------------------------------------------------------------------------
 */

/* Select light stuff with the following defines */
#define TWO_LIGHTS
#define ALL_STUFF
//#define DIFFUSE_COLOR

#ifdef DIFFUSE_COLOR
static void
 hsv_to_rgb(double h, double s, double v,
	    double *r, double *g, double *b)
{
    double xh = fmod(h * 360., 360) / 60.0,
	   i = floor(xh),
	   f = xh - i,
	   p1 = v * (1 - s),
	   p2 = v * (1 - (s * f)),
	   p3 = v * (1 - (s * (1 - f)));

    switch ((int) i)
    {
    case 0:
	*r = v;
	*g = p3;
	*b = p1;
	break;
    case 1:
	*r = p2;
	*g = v;
	*b = p1;
	break;
    case 2:
	*r = p1;
	*g = v;
	*b = p3;
	break;
    case 3:
	*r = p1;
	*g = p2;
	*b = v;
	break;
    case 4:
	*r = p3;
	*g = p1;
	*b = v;
	break;
    case 5:
	*r = v;
	*g = p1;
	*b = p2;
	break;
    }
}
#endif				/* DIFFUSE_COLOR */

/*-------------------------------------------------------------*/
static void spheric_camera(text3dstruct * tp,
			   float center_x, float center_y, float center_z,
			   float phi, float theta, float radius)
{
    float x = center_x + cos(phi) * cos(theta) * radius;
    float y = center_y + sin(phi) * cos(theta) * radius;
    float z = center_z + sin(theta) * radius;

    float vx = -cos(phi) * sin(theta) * radius;
    float vy = -sin(phi) * sin(theta) * radius;
    float vz = cos(theta) * radius;

    glViewport(0, 0, tp->WinW, tp->WinH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, GLfloat(tp->WinW) / GLfloat(tp->WinH), 10, 10000);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(x, y, z, center_x, center_y, center_z, vx, vy, vz);
}

/*-------------------------------------------------------------*/
class GLTTGlyphTriangles:public GLTTGlyphTriangulator
{
public:
    struct Triangle
    {
	FTGlyphVectorizer::POINT * p1;
	FTGlyphVectorizer::POINT * p2;
	FTGlyphVectorizer::POINT * p3;
    };

    Triangle *triangles;
    int nTriangles;

    GLTTboolean count_them;

    GLTTGlyphTriangles(FTGlyphVectorizer * vectorizer):
	GLTTGlyphTriangulator(vectorizer)
	{
	    triangles  = 0;
	    nTriangles = 0;
	    count_them = GLTT_TRUE;
	}
    virtual ~GLTTGlyphTriangles()
    {
	delete[]triangles;
	triangles = 0;
    }
    void alloc()
    {
	delete triangles;
	triangles = new Triangle[nTriangles + 1];
    }
    virtual void triangle(FTGlyphVectorizer::POINT * p1,
			  FTGlyphVectorizer::POINT * p2,
			  FTGlyphVectorizer::POINT * p3)
    {
	if (count_them)
	{
	    ++nTriangles;
	    return;
	}
	triangles[nTriangles].p1 = p1;

	triangles[nTriangles].p2 = p2;
	triangles[nTriangles].p3 = p3;
	++nTriangles;
    }
};

/*
 *-----------------------------------------------------------------------------
 *    Animation functions.
 *-----------------------------------------------------------------------------
 */

/* defines for min/max/evolution factor of animations */
#define FAC_CAMERA	1.15
#define MAX_CAMERA	5.00

#define MIN_EXTRUSION	 25.0
#define MAX_EXTRUSION	305.0
#define FAC_EXTRUSION	  5.0

#define FAC_FREQ	0.15
#define FAC_AMPL	1.5

#define FAC_RAND	15

#ifdef __cplusplus
extern "C" {
#endif

/*-------------------------------------------------------------*/
static void
 t3d_anim_default(text3dstruct * tp)
{
    tp->phi   += tp->direction * angle_speed;
    tp->theta += tp->direction * angle_speed;
}

/*-------------------------------------------------------------*/
static void
 t3d_anim_default2(text3dstruct * tp)
{
    tp->phi   += tp->direction * angle_speed;
    tp->theta += tp->direction * angle_speed * 2.0 ;
}

/*-------------------------------------------------------------*/
static void
 t3d_anim_none(text3dstruct * tp)
{
}

/*-------------------------------------------------------------*/
static void
 t3d_anim_crazy(text3dstruct * tp)
{
    int key = NRAND(32);

    switch (key)
    {
    case 0:
    case 1:
    case 2:
    case 3:
	tp->theta += angle_speed;
	break;
    case 4:
    case 5:
    case 6:
    case 7:
	tp->theta -= angle_speed;
	break;
    case 8:
    case 9:
    case 10:
    case 11:
	tp->phi -= angle_speed;
	break;
    case 12:
    case 13:
    case 14:
    case 15:
	tp->phi += angle_speed;

    case 16:
    case 17:
	if (tp->camera_dist / FAC_CAMERA > tp->ref_camera_dist)
	    tp->camera_dist /= FAC_CAMERA;
	break;
    case 18:
    case 19:
	if (tp->camera_dist * FAC_CAMERA < (tp->ref_camera_dist * MAX_CAMERA))
	    tp->camera_dist *= FAC_CAMERA;
	break;
    case 20:
	if ((tp->extrusion - FAC_EXTRUSION) > MIN_EXTRUSION)
	    tp->extrusion -= FAC_EXTRUSION;
	break;
    case 21:
	if ((tp->extrusion + FAC_EXTRUSION) < MAX_EXTRUSION)
	    tp->extrusion += FAC_EXTRUSION;
	break;
    case 22:
    case 23:
	tp->rampl /= FAC_AMPL;
	break;
    case 24:
    case 25:
	tp->rampl *= FAC_AMPL;
	break;
    case 26:
    case 27:
	tp->rfreq *= FAC_FREQ;
	break;
    case 28:
    case 29:
	tp->rfreq /= FAC_FREQ;
	break;
    }
}

static void
 t3d_anim_updown(text3dstruct * tp)
{
    if (tp->direction > 0)
    {
        if (tp->camera_dist / FAC_CAMERA > tp->ref_camera_dist)
            tp->camera_dist /= FAC_CAMERA;
        else
	    tp->direction *=-1;
    }
    else
    {
        if (tp->camera_dist * FAC_CAMERA < (tp->ref_camera_dist * MAX_CAMERA))
            tp->camera_dist *= FAC_CAMERA;
        else
	    tp->direction *=-1;
    }
}

static void
 t3d_anim_extrusion(text3dstruct * tp)
{
    if (tp->direction > 0)
    {
        if ((tp->extrusion - FAC_EXTRUSION) > MIN_EXTRUSION)
            tp->extrusion -= FAC_EXTRUSION;
        else
            tp->direction *=-1;
    }
    else
    {
        if ((tp->extrusion + FAC_EXTRUSION) < MAX_EXTRUSION)
            tp->extrusion += FAC_EXTRUSION;
        else
            tp->direction *=-1;
    }
}

static void
 t3d_anim_rotatexy(text3dstruct * tp)
{
    tp->phi += tp->direction * angle_speed;
}

static void
 t3d_anim_rotateyz(text3dstruct * tp)
{
    tp->theta += tp->direction * angle_speed;
}

static void
 t3d_anim_frequency(text3dstruct * tp)
{
    /* Better visual if freq is a small value < 0.05 */
    if (tp->direction > 0)
    {
        tp->rfreq /= FAC_FREQ;
    }
    else
    {
        tp->rfreq *= FAC_FREQ;
    }

    if (NRAND(100) < FAC_RAND )
            tp->direction *=-1;
}

static void
 t3d_anim_amplitude(text3dstruct * tp)
{
    if (tp->direction > 0)
    {
        tp->rampl /= FAC_AMPL;
    }
    else
    {
        tp->rampl *= FAC_AMPL;
    }

    if (NRAND(100) < FAC_RAND )
            tp->direction *=-1;
}

static void
 t3d_anim_fullrandom(text3dstruct * tp)
{
    static int animation=NRAND(anims);
    if (NRAND(100) < FAC_RAND/5 || animation <= 1)
    {
    	/* change animation */
      	animation = NRAND(anims);
    }
    anim_array[animation](tp);
}

static void
 t3d_anim_wander(text3dstruct * tp)
{
        static int frame = 0;
	GLfloat x, y, z;
#define SINOID(SCALE,SIZE) \
 ((((1 + sin((frame * (SCALE)) / 2 * M_PI)) / 2.0) * (SIZE)) - (SIZE)/2)

	x = SINOID(0.061, angle_speed);
	y = SINOID(0.083, angle_speed * 2.0);
	z = SINOID(0.075, MAX_CAMERA * 10.0);
	frame++;
    	tp->phi   += tp->direction * x;
    	tp->theta += tp->direction * y;
        tp->camera_dist += tp->direction * z;
}

#ifdef __cplusplus
}
#endif


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/
/*
 *-----------------------------------------------------------------------------
 *    "Main" local funcs.
 *-----------------------------------------------------------------------------
 */

void
 reshape_text3d(ModeInfo * mi, int width, int height)
{
    text3dstruct *tp = &text3d[MI_SCREEN(mi)];

    glViewport(0, 0, tp->WinW = (GLint) width, tp->WinH = (GLint) height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (GLdouble) width / (GLdouble) height, 10, 10000);
    glMatrixMode(GL_MODELVIEW);
}

/*-------------------------------------------------------------*/
static void
 Animate(text3dstruct * tp)
{
    anim_array[tp->animation](tp);
}

/*-------------------------------------------------------------*/
static void
 Draw(text3dstruct * tp,
      Display * display,
      Window window)
{
    int text_length;
    char *c_text;

    if (!nosplit)
    {
	text_length = index_dir(tp->words, (char *) " ");
	if (text_length == 0)
	    text_length = strlen(tp->words);
	if ((c_text = (char *) malloc(text_length)) != NULL)
	strncpy(c_text, tp->words, text_length);
    }
    else
    {
	c_text = tp->words_start;
	text_length = strlen(tp->words_start);
    }
    GLTTFont font(tp->face);

    if (!font.create(DEF_FONTSIZE))
	return;

    FTGlyphVectorizer *vec = new FTGlyphVectorizer[text_length];
    GLTTGlyphTriangles **tri = new GLTTGlyphTriangles *[text_length];

    int i;

    for (i = 0; i < text_length; ++i)
	tri[i] = new GLTTGlyphTriangles(vec + i);

    if (tp->camera_dist == 0.0)
	/* PURIFY reports an Array Bounds Read on the next line */
	tp->ref_camera_dist = tp->camera_dist = font.getWidth(c_text) * 0.75;
    double min_y = 1e20;
    double max_y = -1e20;
    double size_x = 0.0;

    for (i = 0; i < text_length; ++i)
    {
	int ch = (unsigned char) c_text[i];

#if ((XMESA_MAJOR_VERSION > 3 ) || (( XMESA_MAJOR_VERSION == 3 ) && (XMESA_MINOR_VERSION > 0 )))
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

	FTGlyph *g = font.getFont()->getGlyph(ch);

	if (g == 0)
	    continue;
	FTGlyphVectorizer & v = vec[i];
	v.setPrecision(10.0);
	/* PURIFY reports an Array Bounds Write on the next line */
	if (!v.init(g))
	    continue;

	size_x += v.getAdvance();

	if (!v.vectorize())
	    continue;

	for (int c = 0; c < v.getNContours(); ++c)
	{
	    FTGlyphVectorizer::Contour * contour = v.getContour(c);
	    if (contour == 0)
		continue;
	    for (int j = 0; j < contour->nPoints; ++j)
	    {
		FTGlyphVectorizer::POINT * point = contour->points + j;
		if (point->y < min_y)
		    min_y = point->y;
		if (point->y > max_y)
		    max_y = point->y;
		point->data = (void *) new double[6];
	    }
	}
	GLTTGlyphTriangles *t = tri[i];

	if (!t->init(g))
	    continue;

	t->count_them = GLTT_TRUE;
	t->nTriangles = 0;
	t->triangulate();

	t->count_them = GLTT_FALSE;
	t->alloc();
	t->nTriangles = 0;
	t->triangulate();
    }

    if (!nosplit)
	free(c_text);
    if (size_x == 0.0)
    {
	(void) fprintf(stderr, "Please give something to draw !\n");
	return;
    }

    double y_delta = (min_y + max_y) / 2. + min_y + 50;

    for (i = 0; i < text_length; ++i)
    {

#if ((XMESA_MAJOR_VERSION > 3 ) || (( XMESA_MAJOR_VERSION == 3 ) && (XMESA_MINOR_VERSION > 0 )))
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

	FTGlyphVectorizer & v = vec[i];

	for (int c = 0; c < v.getNContours(); ++c)
	{
	    FTGlyphVectorizer::Contour * contour = v.getContour(c);
	    if (contour == 0)
		continue;
	    for (int j = 0; j < contour->nPoints; ++j)
	    {
		FTGlyphVectorizer::POINT * point = contour->points + j;
		point->y -= y_delta;
	    }
	}
    }


#ifdef ALL_STUFF
    float front_emission[4] = {0.1, 0.1, 0.1, 0};
    float front_ambient[4]  = {0.2, 0.2, 0.2, 0};
    float front_diffuse[4]  = {0.95, 0.95, 0.8, 0};
    float back_diffuse[4]   = {0.75, 0.75, 0.95, 0};
    float front_specular[4] = {0.6, 0.6, 0.6, 0};

    glMaterialfv(GL_FRONT, GL_EMISSION, front_emission);
    glMaterialfv(GL_FRONT, GL_AMBIENT, front_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, front_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, front_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 32.0);
#ifdef TWO_LIGHTS
    float light1_ambient[4]  = {0.3, 0.3, 0.3, 1};
    float light1_diffuse[4]  = {0.9, 0.9, 0.9, 1};	/* A "white" light */
    float light1_specular[4] = {0.7, 0.7, 0.7, 1};
    float light1_position[4] = {-1, 1, 1, 0};

    glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);
    glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
    glEnable(GL_LIGHT1);

    float light2_ambient[4]  = {0.1, 0.1, 0.1, 1};
    float light2_diffuse[4]  = {0.85, 0.3, 0.3, 1};	/* A "red" light */
    float light2_specular[4] = {0.6, 0.6, 0.6, 1};
    float light2_position[4] = {1, -1, -1, 0};

    glLightfv(GL_LIGHT2, GL_AMBIENT, light2_ambient);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, light2_specular);
    glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
    glEnable(GL_LIGHT2);
#else
    GLfloat pos[4] = {-1.0, 1.0, 1.0, 0.0};

    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_LIGHT0);
#endif				/* TWO_LIGHTS */

    float back_color[4] = {0.2, 0.2, 0.6, 0};

    glMaterialfv(GL_BACK, GL_DIFFUSE, back_color);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_CULL_FACE);

    glColorMaterial(GL_FRONT, GL_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
#endif				/* ALL_STUFF */

    spheric_camera(tp, tp->center_x,
		   tp->center_y + size_x / 2.,
		   0,
		   tp->phi, tp->theta + M_PI / 2, tp->camera_dist);
    glClearColor(0, 0, 0, 0);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

#ifdef ALL_STUFF
    glEnable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
#endif

    double base_x = 0.0;

    for (i = 0; i < text_length; ++i)
    {

#if ((XMESA_MAJOR_VERSION > 3 ) || (( XMESA_MAJOR_VERSION == 3 ) && (XMESA_MINOR_VERSION > 0 )))
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

	FTGlyphVectorizer & v = vec[i];

	int c;

	for (c = 0; c < v.getNContours(); ++c)
	{
	    FTGlyphVectorizer::Contour * contour = v.getContour(c);
	    if (contour == 0)
		continue;

	    for (int j = 0; j < contour->nPoints; ++j)
	    {
		FTGlyphVectorizer::POINT * point = contour->points + j;
		double cx = -point->y;
		double cy = base_x + point->x;
		double phi = sin(cy * tp->rfreq) * tp->rampl * M_PI / 2.;
		double rcx = cx * cos(phi);
		double rcz = cx * sin(phi);

		double *p = (double *) point->data;
		double *n = p + 3;

		p[0] = rcx;
		p[1] = cy;
		p[2] = rcz;

		n[0] = -sin(phi);
		n[1] = 0.;
		n[2] = cos(phi);
	    }
	}

	GLTTGlyphTriangles::Triangle * triangles = tri[i]->triangles;
	int nTriangles = tri[i]->nTriangles;

	glBegin(GL_TRIANGLES);

	for (int j = 0; j < nTriangles; ++j)
	{
	    GLTTGlyphTriangles::Triangle & t = triangles[j];

	    double *p1 = ((double *) t.p1->data);
	    double *p2 = ((double *) t.p2->data);
	    double *p3 = ((double *) t.p3->data);
	    double *n1 = p1 + 3;
	    double *n2 = p2 + 3;
	    double *n3 = p3 + 3;

#ifdef ALL_STUFF
	    glColor4fv(front_diffuse);
#endif

	    glNormal3dv(n1);
	    glVertex3dv(p1);
	    glNormal3dv(n2);
	    glVertex3dv(p2);
	    glNormal3dv(n3);
	    glVertex3dv(p3);

#ifdef ALL_STUFF
	    glColor4fv(back_diffuse);
#endif

	    glNormal3d(-n3[0], 0., -n3[2]);
	    glVertex3d(p3[0] - n3[0] * tp->extrusion,
		       p3[1],
		       p3[2] - n3[2] * tp->extrusion);
	    glNormal3d(-n2[0], 0., -n2[2]);
	    glVertex3d(p2[0] - n2[0] * tp->extrusion,
		       p2[1],
		       p2[2] - n2[2] * tp->extrusion);
	    glNormal3d(-n1[0], 0., -n1[2]);
	    glVertex3d(p1[0] - n1[0] * tp->extrusion,
		       p1[1],
		       p1[2] - n1[2] * tp->extrusion);
	}
	glEnd();

	for (c = 0; c < v.getNContours(); ++c)
	{
	    FTGlyphVectorizer::Contour * contour = v.getContour(c);
	    if (contour == 0)
		continue;
	    glBegin(GL_QUAD_STRIP);
	    for (int j = 0; j <= contour->nPoints; ++j)
	    {
		int j1 = (j < contour->nPoints) ? j : 0;
		int j0 = (j1 == 0) ? (contour->nPoints - 1) : (j1 - 1);

		FTGlyphVectorizer::POINT * point0 = contour->points + j0;
		FTGlyphVectorizer::POINT * point1 = contour->points + j1;
		double *p0 = (double *) point0->data;
		double *p1 = (double *) point1->data;
		double *e = p0 + 3;
		double vx = p1[0] - p0[0];
		double vy = p1[1] - p0[1];
		double vz = p1[2] - p0[2];
		double nx = -vy * e[2];
		double ny = e[2] * vx - vz * e[0];
		double nz = e[0] * vy;
#ifdef DIFFUSE_COLOR
		double u = double ((j * 2) % contour->nPoints) / double (contour->nPoints);
		double r, g, b;

		hsv_to_rgb(u, 0.7, 0.7, &r, &g, &b);
		glColor4f(r, g, b, 1);	// diffuse color of material
#else
		GLfloat blue[4] = {0.35, 0.35, 1.0, 1.0};
		glColor4fv(blue);
#endif

		glNormal3f(nx, ny, nz);
		glVertex3f(p0[0] - e[0] * tp->extrusion,
			   p0[1],
			   p0[2] - e[2] * tp->extrusion);
		glNormal3f(nx, ny, nz);
		glVertex3f(p0[0], p0[1], p0[2]);
	    }
	    glEnd();
	}

	base_x += v.getAdvance();
    }

#ifdef ALL_STUFF
    glDisable(GL_NORMALIZE);
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);
#endif
    glPopMatrix();

    glXSwapBuffers(display, window);

    for (i = 0; i < text_length; ++i)
    {

#if ((XMESA_MAJOR_VERSION > 3 ) || (( XMESA_MAJOR_VERSION == 3 ) && (XMESA_MINOR_VERSION > 0 )))
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

	delete tri[i];

	FTGlyphVectorizer & v = vec[i];
	for (int c = 0; c < v.getNContours(); ++c)
	{
	    FTGlyphVectorizer::Contour * contour = v.getContour(c);
	    if (contour == 0)
		continue;
	    for (int j = 0; j < contour->nPoints; ++j)
	    {
		FTGlyphVectorizer::POINT * point = contour->points + j;
		delete[](double *) point->data;
		point->data = 0;
	    }
	}
    }

    delete[]tri;
    delete[]vec;
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
 *    Initialize text3d.  Called each time the window changes.
 *-----------------------------------------------------------------------------
 */

void
 init_text3d(ModeInfo * mi)
{
    int i;
    text3dstruct *tp;

    if (text3d == NULL)
    {
	if ((text3d = (text3dstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof(text3dstruct))) == NULL)
	    return;
    }
    tp = &text3d[MI_SCREEN(mi)];
    tp->wire = MI_IS_WIREFRAME(mi);
    tp->extrusion = extrusion;
    tp->rampl = rampl;
    tp->rfreq = rfreq;
    tp->camera_dist = 0.0;

    /* Get animation function */
    tp->animation = 0;	/* Not found equals "Random" */
    tp->direction = (LRAND() & 1) ? 1 : -1; /* random direction */
    tp->rampl *= tp->direction;
    tp->rfreq *= tp->direction;
    for(i=0;anim_names[i] != NULL;i++)
    {
        if ( !strcmp( anim_names[i], animate ) )
        {
    		tp->animation = i;
		break;
        }
    }
    if (!tp->animation)
    {
	/* Random !!! */
    	tp->animation = NRAND(anims);
    }
    else
    {
        tp->animation --;
    }

    if (MI_IS_DEBUG(mi))
    {
	(void) fprintf(stderr,
		   "%s:\n\ttp->animation[%d]=%s\n",
		   MI_NAME(mi), tp->animation, anim_names[tp->animation+1]);
    }

    tp->counter = 0;

	fontfile = getModeFont(mode_font);
	if (!fontfile) {
		MI_CLEARWINDOW(mi);
		release_text3d(mi);
		return;
    }
    tp->face = new FTFace;
    if (!tp->face || !tp->face->open(fontfile)) {
		(void) fprintf(stderr, "%s: unable to open True Type font %s!\n", MI_NAME(mi), fontfile);
		MI_CLEARWINDOW(mi);
		release_text3d(mi);
		return;
    }
    if (MI_IS_DEBUG(mi)) {
		(void) fprintf(stderr,
			"%s:\n\tfontfile=%s .\n", MI_NAME(mi), fontfile);
    }

    /* Do not free fontfile getModeFont handles potential leak */
    if ((tp->glx_context = init_GL(mi)) != NULL)
    {

	reshape_text3d(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
	/*glDrawBuffer(GL_BACK); */
	if (MI_IS_DEBUG(mi))
	{
	    (void) fprintf(stderr,
			   "%s:\n\tcamera_dist=%.1f\n\ttheta=%.1f\n\tphi=%.1f\n\textrusion=%.1f\n\trampl=%.1f.\n\trfreq=%.1f\n\tdirection=%d\n",
			   MI_NAME(mi), tp->camera_dist, tp->theta, tp->phi,
			   tp->extrusion, tp->rampl,tp->rfreq,tp->direction);
	}
/*
	glXSwapBuffers(display, window);
*/

    }
    else
    {
	MI_CLEARWINDOW(mi);
    }

    /* Initialize displayed string */
    tp->words_start = tp->words =
	getWords(MI_SCREEN(mi), MI_NUM_SCREENS(mi));
    if (MI_IS_DEBUG(mi))
    {
	(void) fprintf(stderr,
		   "%s words:\n%s\n",
		   MI_NAME(mi), tp->words);
    }

}

/*
 *-----------------------------------------------------------------------------
 *    Called by the mainline code periodically to update the display.
 *-----------------------------------------------------------------------------
 */
void
 draw_text3d(ModeInfo * mi)
{
    Display *display = MI_DISPLAY(mi);
    Window window = MI_WINDOW(mi);
    text3dstruct *tp;

    if (text3d == NULL) {
	  return;
	}
    tp = &text3d[MI_SCREEN(mi)];

    MI_IS_DRAWN(mi) = True;
    if (!tp->glx_context)
	return;
    tp->counter = tp->counter + 1;
    if (tp->counter > MI_CYCLES(mi) & !nosplit)
    {
	int text_length = index_dir(tp->words, (char *) " ");

	/* Every now and then, get a new word */
	if (text_length == 0)
	    text_length = strlen(tp->words);
	tp->counter = 0;
	tp->words += text_length;
	text_length = strlen(tp->words);
	if (text_length == 0)
	{
	    tp->words_start = tp->words =
		getWords(MI_SCREEN(mi), MI_NUM_SCREENS(mi));
	}
    }
    glDrawBuffer(GL_BACK);
    glXMakeCurrent(display, window, *(tp->glx_context));

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Draw(tp, display, window);
    Animate(tp);
    if (MI_IS_DEBUG(mi))
    {
	(void) fprintf(stderr,
		       "%s:\n\tcamera_dist=%.1f\n\ttheta=%.1f\n\tphi=%.1f\n\textrusion=%.1f\n\trampl=%.1f\n\trfreq=%.1f\n\tdirection=%d\n",
		       MI_NAME(mi), tp->camera_dist, tp->theta, tp->phi,
		       tp->extrusion, tp->rampl,tp->rfreq,tp->direction);
    }
}

/*
 *-----------------------------------------------------------------------------
 *    The display is being taken away from us.  Free up malloc'ed
 *      memory and X resources that we've alloc'ed.  Only called
 *      once, we must zap everything for every screen.
 *-----------------------------------------------------------------------------
 */

void
 release_text3d(ModeInfo * mi)
{
    if (text3d != NULL)
    {
	for (int screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
	{
	    text3dstruct *tp = &text3d[screen];

	    if (tp->face)
		delete tp->face;
	}
	free(text3d);
	text3d = (text3dstruct *) NULL;
    }
    FreeAllGL(mi);
}

void
 refresh_text3d(ModeInfo * mi)
{
    /* Do nothing, it will refresh by itself :) */
}

void
 change_text3d(ModeInfo * mi)
{
    text3dstruct *tp;

    if (text3d == NULL) {
	  return;
	}
    tp = &text3d[MI_SCREEN(mi)];

    if (!tp->glx_context)
	return;
    glDrawBuffer(GL_BACK);
    glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(tp->glx_context));
}

#endif				/* MODE_text3d */
