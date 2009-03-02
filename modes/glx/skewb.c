/* -*- Mode: C; tab-width: 4 -*- */
/* skewb --- Shows an auto-solving Skewb */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)skewb.c	5.00 2000/11/01 xlockmore";

#endif

#undef DEBUG_LISTS
#undef HACK /* I am just doing experiments here to figure it out */
/* #define HACK */

/*-
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
 * This mode shows an auto-solving a skewb "puzzle".
 *
 * Thanks goes also to Brian Paul for making it possible and inexpensive
 * to use OpenGL at home.
 *
 * Based on rubik.c by Marcelo F. Vianna
 *
 * Revision History:
 * 05-Apr-2002: Removed all gllist uses (fix some bug with nvidia driver)
 * 01-Nov-2000: Allocation checks
 * 27-Apr-2000: Started writing, only have corners drawn and algorithm
 *              compiled in.
 */

/*-
 * Color labels mapping:
 * =====================
 *
 *        +------+
 *        |3    0|
 *        |      |
 *        | TOP  |
 *        | (0)  |
 *        |      |
 *        |2    1|
 * +------+------+------+
 * |3    0|3    0|3    0|
 * |      |      |      |
 * | LEFT |FRONT |RIGHT |
 * | (1)  | (2)  | (3)  |
 * |      |      |      |
 * |2    1|2    1|2    1|
 * +------+------+------+
 *        |3    0|
 *        |      |
 *        |BOTTOM|
 *        | (4)  |
 *        |      |
 *        |2    1|
 *        +------+         +------+
 *        |3    0|         |3 /\ 0|
 *        |      |         | /  \ |
 *        | BACK |         |/xxxx\|
 *        | (5)  |         |\(N) /|
 *        |      |         | \  / |
 *        |2    1|         |2 \/ 1|
 *        +------+         +------+
 *
 *  Map to 3d
 *  FRONT  => X, Y
 *  BACK   => X, Y
 *  LEFT   => Z, Y
 *  RIGHT  => Z, Y
 *  TOP    => X, Z
 *  BOTTOM => X, Z
 */

#ifdef VMS
/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */
#include <X11/Intrinsic.h>
#endif

#ifdef STANDALONE
#define MODE_skewb
#define PROGCLASS "Skewb"
#define HACK_INIT init_skewb
#define HACK_DRAW draw_skewb
#define skewb_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: -30 \n" \
 "*cycles: 5 \n"
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#include "vis.h"
#endif /* !STANDALONE */

#ifdef MODE_skewb

#define DEF_HIDESHUFFLING     "False"

static Bool hideshuffling;

static XrmOptionDescRec opts[] =
{
	{(char *) "-hideshuffling", (char *) ".skewb.hideshuffling", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+hideshuffling", (char *) ".skewb.hideshuffling", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(void *) & hideshuffling, (char *) "hideshuffling", (char *) "Hideshuffling", (char *) DEF_HIDESHUFFLING, t_Bool}
};

static OptionStruct desc[] =
{
	{(char *) "-/+hideshuffling", (char *) "turn on/off hidden shuffle phase"}
};

ModeSpecOpt skewb_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   skewb_description =
{"skewb", "init_skewb", "draw_skewb", "release_skewb",
 "draw_skewb", "change_skewb", (char *) NULL, &skewb_opts,
 100000, -30, 5, 1, 64, 1.0, "",
 "Shows an auto-solving Skewb", 0, NULL};

#endif

#define VectMul(X1,Y1,Z1,X2,Y2,Z2) (Y1)*(Z2)-(Z1)*(Y2),(Z1)*(X2)-(X1)*(Z2),(X1)*(Y2)-(Y1)*(X2)
#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi                         M_PI
#endif


#define ACTION_SOLVE    1
#define ACTION_SHUFFLE  0

#define DELAY_AFTER_SHUFFLING  5
#define DELAY_AFTER_SOLVING   20

/*************************************************************************/

#define Scale4Window               (0.9/3.0)
#define Scale4Iconic               (2.1/3.0)

#define MAXORIENT 4		/* Number of orientations of a square */
#define MAXFACES 6		/* Number of faces */

/* Directions relative to the face of a cubie */
#define IGNORE (-1)
#define TR 0
#define BR 1
#define BL 2
#define TL 3
#define STRT 4
#define CW 5
#define HALF 6
#define CCW 7
#define TOP 8
#define RIGHT 9
#define BOTTOM 10
#define LEFT 11
#define MAXROTATE 3
#define MAXCUBES (MAXORIENT+1)
#define MINOR 0
#define MAJOR 1
#define MAXFACES 6

#define TOP_FACE 0
#define LEFT_FACE 1
#define FRONT_FACE 2
#define RIGHT_FACE 3
#define BOTTOM_FACE 4
#define BACK_FACE 5
#define NO_FACE (MAXFACES)
#define NO_ROTATION (2*MAXORIENT)

#define CUBELEN 0.50
#define CUBEROUND (CUBELEN-0.05)
#define STICKERLONG (CUBEROUND-0.05)
#define STICKERSHORT (STICKERLONG-0.05)
#define STICKERDEPTH (CUBELEN+0.01)

#define ObjCubit        0
#define ObjFacit        1
#define MaxObj          2

typedef struct _SkewbLoc {
	int         face;
	int         rotation;	/* Not used yet */
} SkewbLoc;
typedef struct _SkewbLocPos {
        int         face, position, direction;
} SkewbLocPos;
typedef struct _RowNext {
        int         face, direction, sideFace;
} RowNext;
typedef struct _SkewbMove {
        int         face, direction;
        int         position;
} SkewbMove;


/*-
 * Pick a face and a direction on face the next face and orientation
 * is then known.
 */
static SkewbLoc slideNextRow[MAXFACES][MAXORIENT][MAXORIENT / 2] =
{
	{
		{
			{2, CW},
			{1, HALF}},
		{
			{5, CCW},
			{1, STRT}},
		{
			{3, STRT},
			{5, CW}},
		{
			{3, HALF},
			{2, CCW}}
	},
	{
		{
			{4, STRT},
			{5, CW}},
		{
			{0, STRT},
			{5, CCW}},
		{
			{2, CCW},
			{0, HALF}},
		{
			{2, CW},
			{4, HALF}}
	},
	{
		{
			{4, CW},
			{1, CCW}},
		{
			{0, CCW},
			{1, CW}},
		{
			{3, CCW},
			{0, CW}},
		{
			{3, CW},
			{4, CCW}}
	},
	{
		{
			{4, HALF},
			{2, CCW}},
		{
			{0, HALF},
			{2, CW}},
		{
			{5, CW},
			{0, STRT}},
		{
			{5, CCW},
			{4, STRT}}
	},
	{
		{
			{5, CW},
			{1, STRT}},
		{
			{2, CCW},
			{1, HALF}},
		{
			{3, HALF},
			{2, CW}},
		{
			{3, STRT},
			{5, CCW}}
	},
	{
		{
			{0, CW},
			{1, CW}},
		{
			{4, CCW},
			{1, CCW}},
		{
			{3, CW},
			{4, CW}},
		{
			{3, CCW},
			{0, CCW}}
	}
};
static SkewbLoc minToMaj[MAXFACES][MAXORIENT] =
{				/* other equivalent mappings possible */
	{
		{3, CW},
		{2, STRT},
		{1, CCW},
		{5, STRT}},
	{
		{2, STRT},
		{4, CCW},
		{5, HALF},
		{0, CW}},
	{
		{3, STRT},
		{4, STRT},
		{1, STRT},
		{0, STRT}},
	{
		{5, HALF},
		{4, CW},
		{2, STRT},
		{0, CCW}},
	{
		{3, CCW},
		{5, STRT},
		{1, CW},
		{2, STRT}},
	{
		{3, HALF},
		{0, STRT},
		{1, HALF},
		{4, STRT}}
};

static SkewbLoc slideNextFace[MAXFACES][MAXORIENT] =
{
	{
		{5, STRT},
		{3, CW},
		{2, STRT},
		{1, CCW}},
	{
		{0, CW},
		{2, STRT},
		{4, CCW},
		{5, HALF}},
	{
		{0, STRT},
		{3, STRT},
		{4, STRT},
		{1, STRT}},
	{
		{0, CCW},
		{5, HALF},
		{4, CW},
		{2, STRT}},
	{
		{2, STRT},
		{3, CCW},
		{5, STRT},
		{1, CW}},
	{
		{4, STRT},
		{3, HALF},
		{0, STRT},
		{1, HALF}}
};

static int  faceToRotate[MAXFACES][MAXORIENT] =
{
	{3, 2, 1, 5},
	{2, 4, 5, 0},
	{3, 4, 1, 0},
	{5, 4, 2, 0},
	{3, 5, 1, 2},
	{3, 0, 1, 4}
};

#ifdef HACK
static SkewbLocPos orthToDiag[MAXFACES][MAXORIENT][MAXORIENT] =
{
	{
		{
			{3, 0, 1},
			{5, 1, 0},
			{3, 0, 3},
			{5, 1, 2}},
		{
			{3, 3, 0},
			{2, 0, 1},
			{3, 3, 2},
			{2, 0, 3}},
		{
			{1, 0, 3},
			{2, 3, 0},
			{1, 0, 1},
			{2, 3, 2}},
		{
			{1, 3, 2},
			{5, 2, 1},
			{1, 3, 0},
			{5, 2, 3}}
	},
	{
		{
			{2, 3, 0},
			{0, 2, 1},
			{2, 3, 2},
			{0, 2, 3}},
		{
			{2, 2, 3},
			{4, 3, 0},
			{2, 2, 1},
			{4, 3, 2}},
		{
			{5, 3, 2},
			{4, 2, 3},
			{5, 3, 0},
			{4, 2, 1}},
		{
			{5, 2, 1},
			{0, 3, 2},
			{5, 2, 3},
			{0, 3, 0}}
	},
	{
		{
			{3, 3, 0},
			{0, 1, 0},
			{3, 3, 2},
			{0, 1, 2}},
		{
			{3, 2, 3},
			{4, 0, 1},
			{3, 2, 1},
			{4, 0, 3}},
		{
			{1, 1, 0},
			{4, 3, 0},
			{1, 1, 2},
			{4, 3, 2}},
		{
			{1, 0, 3},
			{0, 2, 1},
			{1, 0, 1},
			{0, 2, 3}}
	},
	{
		{
			{5, 1, 2},
			{0, 0, 3},
			{5, 1, 0},
			{0, 0, 1}},
		{
			{5, 0, 1},
			{4, 1, 2},
			{5, 0, 3},
			{4, 1, 0}},
		{
			{2, 1, 0},
			{4, 0, 1},
			{2, 1, 2},
			{4, 0, 3}},
		{
			{2, 0, 3},
			{0, 1, 0},
			{2, 0, 1},
			{0, 1, 2}}
	},
	{
		{
			{3, 2, 3},
			{2, 1, 0},
			{3, 2, 1},
			{2, 1, 2}},
		{
			{3, 1, 2},
			{5, 0, 1},
			{3, 1, 0},
			{5, 0, 3}},
		{
			{1, 2, 1},
			{5, 3, 0},
			{1, 2, 3},
			{5, 3, 2}},
		{
			{1, 1, 0},
			{2, 2, 1},
			{1, 1, 2},
			{2, 2, 3}}
	},
	{
		{
			{3, 1, 2},
			{4, 1, 0},
			{3, 1, 0},
			{4, 1, 2}},
		{
			{3, 0, 1},
			{0, 0, 1},
			{3, 0, 3},
			{0, 0, 3}},
		{
			{1, 3, 2},
			{0, 3, 0},
			{1, 3, 0},
			{0, 3, 2}},
		{
			{1, 2, 1},
			{4, 2, 1},
			{1, 2, 3},
			{4, 2, 3}}
	}
};
#endif

typedef struct {
	GLint       WindH, WindW;
	GLfloat     step;
	SkewbMove  *moves;
	int         storedmoves;
	int         shufflingmoves;
	int         action;
	int         done;
	GLfloat     anglestep;
	SkewbLoc    cubeLoc[MAXFACES][MAXCUBES];
	SkewbLoc    rowLoc[MAXORIENT][MAXCUBES];
	SkewbLoc    minorLoc[MAXORIENT], majorLoc[MAXORIENT][MAXORIENT];
	SkewbMove   movement;
	GLfloat     rotatestep;
	GLfloat     PX, PY, VX, VY;
	GLXContext *glx_context;
	Bool        AreObjectsDefined[2];
} skewbstruct;

static float front_shininess[] =
{60.0};
static float front_specular[] =
{0.7, 0.7, 0.7, 1.0};
static float ambient[] =
{0.0, 0.0, 0.0, 1.0};
static float diffuse[] =
{1.0, 1.0, 1.0, 1.0};
static float position0[] =
{1.0, 1.0, 1.0, 0.0};
static float position1[] =
{-1.0, -1.0, 1.0, 0.0};
static float lmodel_ambient[] =
{0.5, 0.5, 0.5, 1.0};
static float lmodel_twoside[] =
{GL_TRUE};

static float MaterialRed[] =
{0.5, 0.0, 0.0, 1.0};
static float MaterialBlue[] =
{0.0, 0.0, 0.5, 1.0};
static float MaterialGreen[] =
{0.0, 0.5, 0.0, 1.0};
static float MaterialPink[] =
{0.9, 0.5, 0.5, 1.0};
static float MaterialYellow[] =
{0.7, 0.7, 0.0, 1.0};

static float MaterialWhite[] =
{0.8, 0.8, 0.8, 1.0};
static float MaterialGray[] =
{0.2, 0.2, 0.2, 1.0};
static float MaterialGray3[] =
{0.3, 0.3, 0.3, 1.0};
static float MaterialGray4[] =
{0.4, 0.4, 0.4, 1.0};
static float MaterialGray5[] =
{0.5, 0.5, 0.5, 1.0};
static float MaterialGray6[] =
{0.6, 0.6, 0.6, 1.0};
static float MaterialGray7[] =
{0.7, 0.7, 0.7, 1.0};

static skewbstruct *skewb = (skewbstruct *) NULL;

static void
pickcolor(int C, int mono)
{
	switch (C) {
		case TOP_FACE:
			if (mono)
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray3);
			else
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialRed);
			break;
		case LEFT_FACE:
			if (mono)
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray5);
			else
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialBlue);
			break;
		case FRONT_FACE:
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
			break;
		case RIGHT_FACE:
			if (mono)
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray4);
			else
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGreen);
			break;
		case BOTTOM_FACE:
			if (mono)
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray7);
			else
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialPink);
			break;
		case BACK_FACE:
			if (mono)
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray6);
			else
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialYellow);
			break;
	}
}

static Bool
draw_stickerless_cubit(skewbstruct * sp)
{
	glBegin(GL_QUADS);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
	/* Edge of cubit */
	glNormal3f(1.00, 1.00, 0.00);
	glVertex3f(CUBEROUND, CUBELEN, -CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, -CUBEROUND);
	glNormal3f(0.00, 1.00, 1.00);
	glVertex3f(-CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(-CUBEROUND, CUBELEN, CUBEROUND);
	glNormal3f(1.00, 0.00, 1.00);
	glVertex3f(CUBELEN, -CUBEROUND, CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, -CUBEROUND, CUBELEN);
	glEnd();
	glBegin(GL_TRIANGLES);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
	/* Put sticker here */
	glNormal3f(0.00, 0.00, 1.00);
	glVertex3f(CUBEROUND, -CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(-CUBEROUND, CUBEROUND, CUBELEN);
	glNormal3f(1.00, 0.00, 0.00);
	glVertex3f(CUBELEN, CUBEROUND, -CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(CUBELEN, -CUBEROUND, CUBEROUND);
	glNormal3f(0.00, 1.00, 0.00);
	glVertex3f(-CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, -CUBEROUND);
	/* Corner of cubit */
	glNormal3f(1.00, 1.00, 1.00);
	glVertex3f(CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, CUBEROUND);

	/* Sharper corners of cubit */
	glNormal3f(-1.00, 1.00, 1.00);
	glVertex3f(-CUBELEN, CUBEROUND, CUBELEN);
	glVertex3f(-CUBELEN, CUBELEN, CUBEROUND);
	glVertex3f(-CUBELEN, CUBEROUND, CUBEROUND);
	glNormal3f(1.00, -1.00, 1.00);
	glVertex3f(CUBEROUND, -CUBELEN, CUBELEN);
	glVertex3f(CUBEROUND, -CUBELEN, CUBEROUND);
	glVertex3f(CUBELEN, -CUBELEN, CUBEROUND);
	glNormal3f(1.00, 1.00, -1.00);
	glVertex3f(CUBELEN, CUBEROUND, -CUBELEN);
	glVertex3f(CUBEROUND, CUBEROUND, -CUBELEN);
	glVertex3f(CUBEROUND, CUBELEN, -CUBELEN);
	glEnd();

	glBegin(GL_POLYGON);
	glNormal3f(-1.00, 1.00, 1.00);
	glVertex3f(-CUBEROUND, CUBEROUND,  CUBELEN);
	glVertex3f(-CUBEROUND, CUBELEN,  CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, -CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, -CUBEROUND);
	glVertex3f(CUBELEN, -CUBEROUND, CUBEROUND);
	glVertex3f(CUBEROUND, -CUBEROUND, CUBELEN);
	glEnd();
	return True;
}

static Bool
draw_stickerless_facit(skewbstruct * sp)
{
	glBegin(GL_QUADS);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialPink);
	/* Edge of facit */
#if 0
	glNormal3f(0.00, 1.00, 1.00);
	glVertex3f(-CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(-CUBEROUND, -CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, -CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, CUBEROUND, CUBELEN);
#endif
#if 0
	glNormal3f(0.00, 0.00, -1.00);
	glVertex3f(-CUBEROUND, CUBEROUND, -CUBELEN);
	glVertex3f(CUBEROUND, CUBEROUND, -CUBELEN);
	glVertex3f(CUBEROUND, -CUBEROUND, -CUBELEN);
	glVertex3f(-CUBEROUND, -CUBEROUND, -CUBELEN);
	glNormal3f(-1.00, 0.00, 0.00);
	glVertex3f(-CUBELEN, -CUBEROUND, CUBEROUND);
	glVertex3f(-CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(-CUBELEN, CUBEROUND, -CUBEROUND);
	glVertex3f(-CUBELEN, -CUBEROUND, -CUBEROUND);
	glNormal3f(1.00, 0.00, 0.00);
	glVertex3f(CUBELEN, -CUBEROUND, -CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, -CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(CUBELEN, -CUBEROUND, CUBEROUND);
	glNormal3f(0.00, -1.00, 0.00);
	glVertex3f(CUBEROUND, -CUBELEN, -CUBEROUND);
	glVertex3f(CUBEROUND, -CUBELEN, CUBEROUND);
	glVertex3f(-CUBEROUND, -CUBELEN, CUBEROUND);
	glVertex3f(-CUBEROUND, -CUBELEN, -CUBEROUND);
	glNormal3f(0.00, 1.00, 0.00);
	glVertex3f(-CUBEROUND, CUBELEN, -CUBEROUND);
	glVertex3f(-CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, -CUBEROUND);

#endif
#if 0
	glNormal3f(0.00, 1.00, 0.00);
	glVertex3f(CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(-CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(-CUBEROUND, CUBELEN, -CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, -CUBEROUND);
#endif
	glEnd();
	return True;
}

static void
draw_cubit(ModeInfo * mi,
	   int back, int front, int left, int right, int bottom, int top)
{
	/* skewbstruct *sp = &skewb[MI_SCREEN(mi)]; */
	int         mono = MI_IS_MONO(mi);

	if (back != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(back, mono);
		glNormal3f(0.00, 0.00, -1.00);
		if (top != NO_FACE) {
			glVertex3f(-STICKERSHORT, STICKERLONG, -STICKERDEPTH);
			glVertex3f(STICKERSHORT, STICKERLONG, -STICKERDEPTH);
		}
		if (left != NO_FACE) {
			glVertex3f(-STICKERLONG, -STICKERSHORT, -STICKERDEPTH);
			glVertex3f(-STICKERLONG, STICKERSHORT, -STICKERDEPTH);
		}
		if (bottom != NO_FACE) {
			glVertex3f(STICKERSHORT, -STICKERLONG, -STICKERDEPTH);
			glVertex3f(-STICKERSHORT, -STICKERLONG, -STICKERDEPTH);
		}
		if (right != NO_FACE) {
			glVertex3f(STICKERLONG, STICKERSHORT, -STICKERDEPTH);
			glVertex3f(STICKERLONG, -STICKERSHORT, -STICKERDEPTH);
		}
		glEnd();
	}
	if (front != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(front, mono);
		glNormal3f(0.00, 0.00, 1.00);
		if (top != NO_FACE) {
			glVertex3f(STICKERSHORT, STICKERLONG, STICKERDEPTH);
			glVertex3f(-STICKERSHORT, STICKERLONG, STICKERDEPTH);
		}
		if (left != NO_FACE) {
			glVertex3f(-STICKERLONG, STICKERSHORT, STICKERDEPTH);
			glVertex3f(-STICKERLONG, -STICKERSHORT, STICKERDEPTH);
		}
		if (bottom != NO_FACE) {
			glVertex3f(-STICKERSHORT, -STICKERLONG, STICKERDEPTH);
			glVertex3f(STICKERSHORT, -STICKERLONG, STICKERDEPTH);
		}
		if (right != NO_FACE) {
			glVertex3f(STICKERLONG, -STICKERSHORT, STICKERDEPTH);
			glVertex3f(STICKERLONG, STICKERSHORT, STICKERDEPTH);
		}
		glEnd();
	}
	if (left != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(left, mono);
		glNormal3f(-1.00, 0.00, 0.00);
		if (front != NO_FACE) {
			glVertex3f(-STICKERDEPTH, -STICKERSHORT, STICKERLONG);
			glVertex3f(-STICKERDEPTH, STICKERSHORT, STICKERLONG);
		}
		if (top != NO_FACE) {
			glVertex3f(-STICKERDEPTH, STICKERLONG, STICKERSHORT);
			glVertex3f(-STICKERDEPTH, STICKERLONG, -STICKERSHORT);
		}
		if (back != NO_FACE) {
			glVertex3f(-STICKERDEPTH, STICKERSHORT, -STICKERLONG);
			glVertex3f(-STICKERDEPTH, -STICKERSHORT, -STICKERLONG);
		}
		if (bottom != NO_FACE) {
			glVertex3f(-STICKERDEPTH, -STICKERLONG, -STICKERSHORT);
			glVertex3f(-STICKERDEPTH, -STICKERLONG, STICKERSHORT);
		}
		glEnd();
	}
	if (right != NO_FACE) { /* Green */
		glBegin(GL_POLYGON);
		pickcolor(right, mono);
		glNormal3f(1.00, 0.00, 0.00);
		if (front != NO_FACE) {
			glVertex3f(STICKERDEPTH, STICKERSHORT, STICKERLONG);
			glVertex3f(STICKERDEPTH, -STICKERSHORT, STICKERLONG);
		}
		if (top != NO_FACE) {
			glVertex3f(STICKERDEPTH, STICKERLONG, -STICKERSHORT);
			glVertex3f(STICKERDEPTH, STICKERLONG, STICKERSHORT);
		}
		if (back != NO_FACE) {
			glVertex3f(STICKERDEPTH, -STICKERSHORT, -STICKERLONG);
			glVertex3f(STICKERDEPTH, STICKERSHORT, -STICKERLONG);
		}
		if (bottom != NO_FACE) {
			glVertex3f(STICKERDEPTH, -STICKERLONG, STICKERSHORT);
			glVertex3f(STICKERDEPTH, -STICKERLONG, -STICKERSHORT);
		}
		glEnd();
	}
	if (bottom != NO_FACE) { /* Pink */
		glBegin(GL_POLYGON);
		pickcolor(bottom, mono);
		glNormal3f(0.00, -1.00, 0.00);
		if (left != NO_FACE) {
			glVertex3f(-STICKERLONG, -STICKERDEPTH, STICKERSHORT);
			glVertex3f(-STICKERLONG, -STICKERDEPTH, -STICKERSHORT);
		}
		if (front != NO_FACE) {
			glVertex3f(STICKERSHORT, -STICKERDEPTH, STICKERLONG);
			glVertex3f(-STICKERSHORT, -STICKERDEPTH, STICKERLONG);
		}
		if (right != NO_FACE) {
			glVertex3f(STICKERLONG, -STICKERDEPTH, -STICKERSHORT);
			glVertex3f(STICKERLONG, -STICKERDEPTH, STICKERSHORT);
		}
		if (back != NO_FACE) {
			glVertex3f(-STICKERSHORT, -STICKERDEPTH, -STICKERLONG);
			glVertex3f(STICKERSHORT, -STICKERDEPTH, -STICKERLONG);
		}
		glEnd();
	}
	if (top != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(top, mono);
		glNormal3f(0.00, 1.00, 0.00);
		if (left != NO_FACE) {
			glVertex3f(-STICKERLONG, STICKERDEPTH, -STICKERSHORT);
			glVertex3f(-STICKERLONG, STICKERDEPTH, STICKERSHORT);
		}
		if (front != NO_FACE) {
			glVertex3f(-STICKERSHORT, STICKERDEPTH, STICKERLONG);
			glVertex3f(STICKERSHORT, STICKERDEPTH, STICKERLONG);
		}
		if (right != NO_FACE) {
			glVertex3f(STICKERLONG, STICKERDEPTH, STICKERSHORT);
			glVertex3f(STICKERLONG, STICKERDEPTH, -STICKERSHORT);
		}
		if (back != NO_FACE) {
			glVertex3f(STICKERSHORT, STICKERDEPTH, -STICKERLONG);
			glVertex3f(-STICKERSHORT, STICKERDEPTH, -STICKERLONG);
		}
		glEnd();
	}
}

#ifdef HACK
static void
draw_facit(ModeInfo * mi,
	   int back, int front, int left, int right, int bottom, int top)
{
	/* skewbstruct *sp = &skewb[MI_SCREEN(mi)]; */
	int         mono = MI_IS_MONO(mi);

	if (back != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(back, mono);
		glNormal3f(0.00, 0.00, -1.00);
		glEnd();
	}
	if (front != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(front, mono);
		glNormal3f(0.00, 0.00, 1.00);
		glEnd();
	}
	if (left != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(left, mono);
		glNormal3f(-1.00, 0.00, 0.00);
		glEnd();
	}
	if (right != NO_FACE) { /* Green */
		glBegin(GL_POLYGON);
		pickcolor(right, mono);
		glNormal3f(1.00, 0.00, 0.00);
		glEnd();
	}
	if (bottom != NO_FACE) { /* Pink */
		glBegin(GL_POLYGON);
		pickcolor(bottom, mono);
		glNormal3f(0.00, -1.00, 0.00);
		glEnd();
	}
	if (top != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(top, mono);
		glNormal3f(0.00, 1.00, 0.00);
		glEnd();
	}
}
#endif

static Bool
draw_cube(ModeInfo * mi)
{
#define S1 1
#define DRAW_STICKERLESS_FACIT(sp) if (!draw_stickerless_facit(sp)) return False
#define DRAW_STICKERLESS_CUBIT(sp) if (!draw_stickerless_cubit(sp)) return False

	skewbstruct *sp = &skewb[MI_SCREEN(mi)];
	SkewbLoc  slice;
	GLfloat     rotatestep;
	/* int         i, j, k; */

	if (sp->movement.face == NO_FACE) {
		slice.face = NO_FACE;
		slice.rotation = NO_ROTATION;
	}
#ifdef FIXME
	 else {
		convertMove(sp, sp->movement, &slice);
	}
#endif
	rotatestep = (slice.rotation == CCW) ? sp->rotatestep : -sp->rotatestep;


/*-
 * The glRotatef() routine transforms the coordinate system for every future
 * vertex specification (this is not so simple, but by now comprehending this
 * is sufficient). So if you want to rotate the inner slice, you can draw
 * one slice, rotate the anglestep for the centerslice, draw the inner slice,
 * rotate reversely and draw the other slice.
 * There is a sequence for drawing cubies for each axis being moved...
 */
	switch (slice.face) {
		case NO_FACE:
		case TOP_FACE:	/* BOTTOM_FACE too */
			glPushMatrix();
			glRotatef(rotatestep, 0, 1, 0);

			glTranslatef(-0.5, -0.5, -0.5);
			/* glTranslatef(S1, 0, S1); */
			DRAW_STICKERLESS_FACIT(sp);
			glPushMatrix();
			glRotatef(90.0, 0, 1, 0);
			glRotatef(180.0, 1, 0, 0);
			DRAW_STICKERLESS_CUBIT(sp);
			glPopMatrix();
			draw_cubit(mi, 0, 6, 2, 6, 4, 6);
			glTranslatef(0, 0, S1);
			glPushMatrix();
			glRotatef(180.0, 0, 0, 1);
			DRAW_STICKERLESS_CUBIT(sp);
			glPopMatrix();
			draw_cubit(mi, 6, 1, 2, 6, 4, 6); /* BL */
			glTranslatef(S1, 0, -S1);
			glPushMatrix();
			glRotatef(90.0, 0, 1, 0);
			glRotatef(90.0, 1, 0, 0);
			DRAW_STICKERLESS_CUBIT(sp);
			glPopMatrix();
			draw_cubit(mi, 0, 6, 6, 3, 4, 6);
			glTranslatef(0, 0, S1);
			glPushMatrix();
			glRotatef(90.0, 1, 0, 0);
			DRAW_STICKERLESS_CUBIT(sp);
			glPopMatrix();
			draw_cubit(mi, 6, 1, 6, 3, 4, 6); /* BR */
			glPopMatrix();
			glPushMatrix();
			glTranslatef(-0.5, 0.5, -0.5);
			glPushMatrix();
			glRotatef(90.0, 0, -1, 0);
			glRotatef(90.0, 0, 0, 1);
			DRAW_STICKERLESS_CUBIT(sp);
			glPopMatrix();
			draw_cubit(mi, 0, 6, 2, 6, 6, 5);
			glTranslatef(0, 0, S1);
			glPushMatrix();
			glRotatef(90.0, 0, 0, 1);
			DRAW_STICKERLESS_CUBIT(sp);
			glPopMatrix();
			draw_cubit(mi, 6, 1, 2, 6, 6, 5); /* UL */
			glTranslatef(S1, 0, -S1);
			glPushMatrix();
			glRotatef(90.0, 0, 1, 0);
			DRAW_STICKERLESS_CUBIT(sp);
			glPopMatrix();
			draw_cubit(mi, 0, 6, 6, 3, 6, 5);
			glTranslatef(0, 0, S1);
			DRAW_STICKERLESS_CUBIT(sp);
			draw_cubit(mi, 6, 1, 6, 3, 6, 5); /* UR */
			glPopMatrix();
			break;
	}
	return True;
#undef S1
}

/* From David Bagley's xskewb.  Used by permission. ;)  */
static void
readDiagonal(skewbstruct *sp, int face, int corner, int orient, int size)
{
	int         g;

	if (size == MINOR)
		sp->minorLoc[orient] = sp->cubeLoc[face][corner];
	else {			/* size == MAJOR */
		for (g = 1; g < MAXORIENT; g++)
			sp->majorLoc[orient][g - 1] =
				sp->cubeLoc[face][(corner + g) % MAXORIENT];
		sp->majorLoc[orient][MAXORIENT - 1] =
			sp->cubeLoc[face][MAXORIENT];
	}
}

static void
rotateDiagonal(skewbstruct *sp, int rotate, int orient, int size)
{
	int         g;

	if (size == MINOR)
		sp->minorLoc[orient].rotation =
			(sp->minorLoc[orient].rotation + rotate) % MAXORIENT;
	else			/* size == MAJOR */
		for (g = 0; g < MAXORIENT; g++)
			sp->majorLoc[orient][g].rotation =
				(sp->majorLoc[orient][g].rotation + rotate) % MAXORIENT;
}

static void
writeDiagonal(skewbstruct *sp, int face, int corner, int orient, int size)
{
	int         g, h;

	if (size == MINOR) {
		sp->cubeLoc[face][corner] = sp->minorLoc[orient];
		/* DrawTriangle(face, corner); */
	} else {		/* size == MAJOR */
		sp->cubeLoc[face][MAXORIENT] =
			sp->majorLoc[orient][MAXORIENT - 1];
		/* DrawDiamond(face); */
		for (g = 1; g < MAXORIENT; g++) {
			h = (corner + g) % MAXORIENT;
			sp->cubeLoc[face][h] = sp->majorLoc[orient][g - 1];
			/* DrawTriangle(face, h); */
		}
	}
}

static void
readFace(skewbstruct * sp, int face, int h)
{
	int         position;

	for (position = 0; position < MAXCUBES; position++)
		sp->rowLoc[h][position] = sp->cubeLoc[face][position];
}

static void
writeFace(skewbstruct * sp, int face, int rotate, int h)
{
	int         corner, newCorner;

	for (corner = 0; corner < MAXORIENT; corner++) {
		newCorner = (corner + rotate) % MAXORIENT;
		sp->cubeLoc[face][newCorner] = sp->rowLoc[h][corner];
		sp->cubeLoc[face][newCorner].rotation =
			(sp->cubeLoc[face][newCorner].rotation + rotate) % MAXORIENT;
		/* DrawTriangle(face, (corner + rotate) % MAXORIENT); */
	}
	sp->cubeLoc[face][MAXORIENT] = sp->rowLoc[h][MAXORIENT];
	sp->cubeLoc[face][MAXORIENT].rotation =
		(sp->cubeLoc[face][MAXORIENT].rotation + rotate) % MAXORIENT;
	/* DrawDiamond(face); */
}

static void
rotateFace(skewbstruct * sp, int face, int direction)
{
	SkewbLoc    faceLoc[MAXCUBES];
	int         corner;

	/* Read Face */
	for (corner = 0; corner < MAXORIENT; corner++)
		faceLoc[corner] = sp->cubeLoc[face][corner];
	/* Write Face */
	for (corner = 0; corner < MAXORIENT; corner++) {
		sp->cubeLoc[face][corner] = (direction == CW) ?
			faceLoc[(corner + MAXORIENT - 1) % MAXORIENT] :
			faceLoc[(corner + 1) % MAXORIENT];
		sp->cubeLoc[face][corner].rotation =
			(sp->cubeLoc[face][corner].rotation + direction) % MAXORIENT;
		/* DrawTriangle(face, corner); */
	}
	sp->cubeLoc[face][MAXORIENT].rotation =
		(sp->cubeLoc[face][MAXORIENT].rotation + direction) % MAXORIENT;
	/* DrawDiamond(face); */
}

#ifdef HACK
static      Boolean
checkMoveDir(int position1, int position2, int *direction)
{
	if (!((position1 - position2 + MAXORIENT) % 2))
		return False;
	switch (position1) {
		case 0:
			*direction = (position2 == 1) ? 2 : 3;
			break;
		case 1:
			*direction = (position2 == 2) ? 3 : 0;
			break;
		case 2:
			*direction = (position2 == 3) ? 0 : 1;
			break;
		case 3:
			*direction = (position2 == 0) ? 1 : 2;
			break;
		default:
			return False;
	}
	*direction += 2 * MAXORIENT;
	return True;
}
#endif

static void
moveSkewb(skewbstruct * sp, int face, int direction, int position)
{
	int	 newFace, newDirection, newCorner, k, size, rotate;

	if (direction < 2 * MAXORIENT) {
		/* position as MAXORIENT is ambiguous */
		for (size = MINOR; size <= MAJOR; size++) {
			readDiagonal(sp, face, position, 0, size);
			for (k = 1; k <= MAXROTATE; k++) {
				newFace = slideNextRow[face][position][direction / 2].face;
				rotate = slideNextRow[face][position][direction / 2].rotation %
					MAXORIENT;
				newDirection = (rotate + direction) % MAXORIENT;
				newCorner = (rotate + position) % MAXORIENT;
				if (k != MAXROTATE)
					readDiagonal(sp, newFace, newCorner, k, size);
				rotateDiagonal(sp, rotate, k - 1, size);
				writeDiagonal(sp, newFace, newCorner, k - 1, size);
				face = newFace;
				position = newCorner;
				direction = newDirection;
			}
			if (size == MINOR) {
				newFace = minToMaj[face][position].face;
				rotate = minToMaj[face][position].rotation % MAXORIENT;
				direction = (rotate + direction) % MAXORIENT;
				position = (position + rotate + 2) % MAXORIENT;
				face = newFace;
			}
		}
	} else {
		rotateFace(sp, faceToRotate[face][direction % MAXORIENT], CW);
		rotateFace(sp, faceToRotate[face][(direction + 2) % MAXORIENT], CCW);
		readFace(sp, face, 0);
		for (k = 1; k <= MAXORIENT; k++) {
			newFace = slideNextFace[face][direction % MAXORIENT].face;
			rotate = slideNextFace[face][direction % MAXORIENT].rotation;
			newDirection = (rotate + direction) % MAXORIENT;
			if (k != MAXORIENT)
				readFace(sp, newFace, k);
			writeFace(sp, newFace, rotate, k - 1);
			face = newFace;
			direction = newDirection;
		}
	}
}

#ifdef DEBUG
void
printCube(skewbstruct * sp)
{
	int	 face, position;

	for (face = 0; face < MAXFACES; face++) {
		for (position = 0; position < MAXCUBES; position++)
			(void) printf("%d %d  ", sp->cubeLoc[face][position].face,
			sp->cubeLoc[face][position].rotation);
		}
		(void) printf("\n");
	}
	(void) printf("\n");
}

#endif

static void
evalmovement(ModeInfo * mi, SkewbMove movement)
{
	skewbstruct *sp = &skewb[MI_SCREEN(mi)];

#ifdef DEBUG
	printCube(sp);
#endif
	if (movement.face < 0 || movement.face >= MAXFACES)
		return;

	moveSkewb(sp, movement.face, movement.direction, movement.position);

}

#ifdef HACK
static      Bool
compare_moves(skewbstruct * sp, SkewbMove move1, SkewbMove move2, Bool opp)
{
#ifdef FIXME
	SkewbLoc  slice1, slice2;

	convertMove(sp, move1, &slice1);
	convertMove(sp, move2, &slice2);
	if (slice1.face == slice2.face) {
		if (slice1.rotation == slice2.rotation) { /* CW or CCW */
			if (!opp)
				return True;
		} else {
			if (opp)
				return True;
		}
	}
#endif
	return False;
}
#endif

static Bool
shuffle(ModeInfo * mi)
{
	skewbstruct *sp = &skewb[MI_SCREEN(mi)];
	int	 i, face, position;
	SkewbMove   move;

	for (face = 0; face < MAXFACES; face++) {
		for (position = 0; position < MAXCUBES; position++) {
			sp->cubeLoc[face][position].face = face;
			sp->cubeLoc[face][position].rotation = TOP;
		}
	}
	sp->storedmoves = MI_COUNT(mi);
	if (sp->storedmoves < 0) {
		if (sp->moves != NULL)
			free(sp->moves);
		sp->moves = (SkewbMove *) NULL;
		sp->storedmoves = NRAND(-sp->storedmoves) + 1;
	}
	if ((sp->storedmoves) && (sp->moves == NULL))
		if ((sp->moves = (SkewbMove *) calloc(sp->storedmoves + 1,
				sizeof (SkewbMove))) == NULL) {
			return False;
		}
	if (MI_CYCLES(mi) <= 1) {
		sp->anglestep = 180.0;
	} else {
		sp->anglestep = 180.0 / (GLfloat) (MI_CYCLES(mi));
	}

	for (i = 0; i < sp->storedmoves; i++) {
		Bool condition;

		do {
			move.face = NRAND(MAXFACES);
			move.direction = NRAND(2);
			move.position = NRAND(MAXORIENT);
			condition = True;
			/*
			 * Some silly moves being made, weed out later....
			 */
		} while (!condition);
		if (hideshuffling)
			evalmovement(mi, move);
		sp->moves[i] = move;
	}
	sp->VX = 0.05;
	if (NRAND(100) < 50)
		sp->VX *= -1;
	sp->VY = 0.05;
	if (NRAND(100) < 50)
		sp->VY *= -1;
	sp->movement.face = NO_FACE;
	sp->rotatestep = 0;
	sp->action = hideshuffling ? ACTION_SOLVE : ACTION_SHUFFLE;
	sp->shufflingmoves = 0;
	sp->done = 0;
	return True;
}

static void
reshape(ModeInfo * mi, int width, int height)
{
	skewbstruct *sp = &skewb[MI_SCREEN(mi)];

	glViewport(0, 0, sp->WindW = (GLint) width, sp->WindH = (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 15.0);
	glMatrixMode(GL_MODELVIEW);

	sp->AreObjectsDefined[ObjFacit] = False;
	sp->AreObjectsDefined[ObjCubit] = False;
}

static Bool
pinit(ModeInfo * mi)
{
	glClearDepth(1.0);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glColor3f(1.0, 1.0, 1.0);

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION, position0);
	glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT1, GL_POSITION, position1);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
	glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glEnable(GL_CULL_FACE);

	glShadeModel(GL_FLAT);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);

	return (shuffle(mi));
}

static void
free_skewb(skewbstruct *sp)
{
	if (sp->moves != NULL) {
		free(sp->moves);
		sp->moves = (SkewbMove *) NULL;
	}
}

void
release_skewb(ModeInfo * mi)
{
	if (skewb != NULL) {
		int	 screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			skewbstruct *sp = &skewb[screen];

			free_skewb(sp);
		}
		free(skewb);
		skewb = (skewbstruct *) NULL;
        }
	FreeAllGL(mi);
}

void
init_skewb(ModeInfo * mi)
{
	skewbstruct *sp;

	if (skewb == NULL) {
		if ((skewb = (skewbstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (skewbstruct))) == NULL)
			return;
	}
	sp = &skewb[MI_SCREEN(mi)];

	sp->step = NRAND(180);
	sp->PX = ((float) LRAND() / (float) MAXRAND) * 2.0 - 1.0;
	sp->PY = ((float) LRAND() / (float) MAXRAND) * 2.0 - 1.0;

	if ((sp->glx_context = init_GL(mi)) != NULL) {

		reshape(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		glDrawBuffer(GL_BACK);
		if (!pinit(mi)) {
			free_skewb(sp);
			if (MI_IS_VERBOSE(mi)) {
				 (void) fprintf(stderr,
					"Could not allocate memory for skewb\n");
			}
		}
	} else {
		MI_CLEARWINDOW(mi);
	}
}

void
draw_skewb(ModeInfo * mi)
{
	Bool bounced = False;
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	skewbstruct *sp;

	if (skewb == NULL)
		return;
	sp = &skewb[MI_SCREEN(mi)];
	if ((sp->storedmoves) && (sp->moves == NULL))
		return;

	MI_IS_DRAWN(mi) = True;
	if (!sp->glx_context)
		return;

	glXMakeCurrent(display, window, *(sp->glx_context));

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	glTranslatef(0.0, 0.0, -10.0);

	sp->PX += sp->VX;
	sp->PY += sp->VY;

	if (sp->PY < -1) {
		sp->PY += (-1) - (sp->PY);
		sp->VY = -sp->VY;
		bounced = True;
	}
	if (sp->PY > 1) {
		sp->PY -= (sp->PY) - 1;
		sp->VY = -sp->VY;
		bounced = True;
	}
	if (sp->PX < -1) {
		sp->PX += (-1) - (sp->PX);
		sp->VX = -sp->VX;
		bounced = True;
	}
	if (sp->PX > 1) {
		sp->PX -= (sp->PX) - 1;
		sp->VX = -sp->VX;
		bounced = True;
	}
	if (bounced) {
		sp->VX += ((float) LRAND() / (float) MAXRAND) * 0.02 - 0.01;
		sp->VX += ((float) LRAND() / (float) MAXRAND) * 0.02 - 0.01;
		if (sp->VX > 0.06)
			sp->VX = 0.06;
		if (sp->VY > 0.06)
			sp->VY = 0.06;
		if (sp->VX < -0.06)
			sp->VX = -0.06;
		if (sp->VY < -0.06)
			sp->VY = -0.06;
	}
	if (!MI_IS_ICONIC(mi)) {
		glTranslatef(sp->PX, sp->PY, 0);
		glScalef(Scale4Window * sp->WindH / sp->WindW,
			Scale4Window, Scale4Window);
	} else {
		glScalef(Scale4Iconic * sp->WindH / sp->WindW,
			Scale4Iconic, Scale4Iconic);
	}
	glRotatef(sp->step * 100, 1, 0, 0);
	glRotatef(sp->step * 95, 0, 1, 0);
	glRotatef(sp->step * 90, 0, 0, 1);
	if (!draw_cube(mi)) {
		release_skewb(mi);
		return;
	}
	glXSwapBuffers(display, window);

#if 0
	if (sp->action == ACTION_SHUFFLE) {
		if (sp->done) {
			if (++sp->rotatestep > DELAY_AFTER_SHUFFLING) {
				sp->movement.face = NO_FACE;
				sp->rotatestep = 0;
				sp->action = ACTION_SOLVE;
				sp->done = 0;
			}
		} else {
			if (sp->movement.face == NO_FACE) {
				if (sp->shufflingmoves < sp->storedmoves) {
					sp->rotatestep = 0;
					sp->movement = sp->moves[sp->shufflingmoves];
				} else {
					sp->rotatestep = 0;
					sp->done = 1;
				}
			} else {
				if (sp->rotatestep == 0) {
					;
				}
				sp->rotatestep += sp->anglestep;
				if (sp->rotatestep > 180) {
					evalmovement(mi, sp->movement);
					sp->shufflingmoves++;
					sp->movement.face = NO_FACE;
				}
			}
		}
	} else {
		if (sp->done) {
			if (++sp->rotatestep > DELAY_AFTER_SOLVING)
				if (!shuffle(mi)) {
					free_skewb(sp);
					if (MI_IS_VERBOSE(mi)) {
						 (void) fprintf(stderr,
							"Could not allocate memory for skewb\n");
					}
				}
		} else {
			if (sp->movement.face == NO_FACE) {
				if (sp->storedmoves > 0) {
					sp->rotatestep = 0;
					sp->movement = sp->moves[sp->storedmoves - 1];
					sp->movement.direction = (sp->movement.direction +
						(MAXORIENT / 2)) % MAXORIENT;
				} else {
					sp->rotatestep = 0;
					sp->done = 1;
				}
			} else {
				if (sp->rotatestep == 0) {
					;
				}
				sp->rotatestep += sp->anglestep;
				if (sp->rotatestep > 180) {
					evalmovement(mi, sp->movement);
					sp->storedmoves--;
					sp->movement.face = NO_FACE;
				}
			}
		}
	}
#endif

	glPopMatrix();

	glFlush();

	sp->step += 0.05;
}

void
change_skewb(ModeInfo * mi)
{
	skewbstruct *sp;

	if (skewb == NULL)
		return;
	sp = &skewb[MI_SCREEN(mi)];

	if (!sp->glx_context)
		return;
	if (!pinit(mi)) {
		free_skewb(sp);
		if (MI_IS_VERBOSE(mi)) {
			 (void) fprintf(stderr,
				"Could not allocate memory for skewb\n");
		}
	}
}

#endif
