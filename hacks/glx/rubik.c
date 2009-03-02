/* -*- Mode: C; tab-width: 4 -*- */
/* rubik --- Shows an auto-solving Rubik's cube */

#if 0
static const char sccsid[] = "@(#)rubik.c	5.01 2001/03/01 xlockmore";
#endif

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
 * This mode shows an auto-solving rubik's cube "puzzle". If somebody
 * intends to make a game or something based on this code, please let me
 * know first, my e-mail address is provided in this comment. Marcelo.
 *
 * Thanks goes also to Brian Paul for making it possible and inexpensive
 * to use OpenGL at home.
 *
 * Since I'm not a native English speaker, my apologies for any grammatical
 * mistakes.
 *
 * My e-mail address is
 * mfvianna@centroin.com.br
 *
 * Marcelo F. Vianna (Jul-31-1997)
 *
 * Revision History:
 * 05-Apr-2002: Removed all gllist uses (fix some bug with nvidia driver)
 * 01-Mar-2001: Added FPS stuff - Eric Lassauge <lassauge@mail.dotcom.fr>
 * 01-Nov-2000: Allocation checks
 * 27-Apr-1999: LxMxN stuff added.
 * 26-Sep-1998: Added some more movement (the cube does not stay in the screen
 *              center anymore. Also fixed the scale problem immediately after
 *              shuffling when the puzzle is solved.
 * 08-Aug-1997: Now has some internals from xrubik by David Bagley
 *              This should make it easier to add features.
 * 02-Aug-1997: Now behaves more like puzzle.c: first show the cube being
 *              shuffled and then being solved. A mode specific option was
 *              added:
 *              "+/-hideshuffling" to provide the original behavior (in which
 *              only the solution is shown).
 *              The color labels corners are now rounded.
 *              Optimized the cubit() routine using glLists.
 * 01-Aug-1997: Shuffling now avoids movements that undoes the previous
 *              movement and three consecutive identical moves (which is
 *              pretty stupid).
 *              improved the "cycles" option in replacement of David's hack,
 *              now rp->anglestep is a GLfloat, so this option selects the
 *              "exact" number of frames that a rotation (movement) takes to
 *              complete.
 * 30-Jul-1997: Initial release, there is no algorithm to solve the puzzle,
 *              instead, it randomly shuffle the cube and then make the
 *              movements in the reverse order.
 *              The mode was written in 1 day (I got sick and had the day off).
 *              There was not much to do since I could not leave home... :)
 */

/*-
 * Color labels mapping:
 * =====================
 *
 *             +-----------+
 *             |0-->       |
 *             ||          |
 *             |v  TOP(0)  |
 *             |           |
 *             |          8|
 * +-----------+-----------+-----------+
 * |0-->       |0-->       |0-->       |
 * ||          ||          ||          |
 * |v  LEFT(1) |v FRONT(2) |v RIGHT(3) |
 * |           |           |           |
 * |          8|          8|          8|
 * +-----------+-----------+-----------+
 *             |0-->       |
 *             ||          |
 *             |v BOTTOM(4)|
 *             |           |
 *             |          8|
 *             +-----------+             +---+---+---+
 *             |0-->       |             | 0 | 1 | 2 |
 *             ||          |             |--xxxxx(N)-+
 *             |v  BACK(5) |             | 3 | 4 | 5 |
 *             |           |             +---+---+---+
 *             |          8|             | 6 | 7 | 8 |
 *             +-----------+             +---+---+---+
 *
 *  Map to 3d
 *  FRONT  => X, Y
 *  BACK   => X, Y
 *  LEFT   => Z, Y
 *  RIGHT  => Z, Y
 *  TOP    => X, Z
 *  BOTTOM => X, Z
 */

#ifdef STANDALONE
# define MODE_rubik
# define PROGCLASS	"Rubik"
# define HACK_INIT	init_rubik
# define HACK_DRAW	draw_rubik
# define HACK_RESHAPE reshape
# define rubik_opts	xlockmore_opts
# define DEFAULTS	"*delay: 40000 \n"		\
					"*count: -30 \n"		\
					"*showFPS: False \n"	\
					"*cycles: 5 \n"			\
					"*size:  -6 \n"
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
# include "vis.h"
#endif /* !STANDALONE */

#ifdef MODE_rubik

#define DEF_SIZEX     "0"
#define DEF_SIZEY     "0"
#define DEF_SIZEZ     "0"
#define DEF_HIDESHUFFLING     "False"

static int sizex;
static int sizey;
static int sizez;
static Bool hideshuffling;

static XrmOptionDescRec opts[] =
{
        {"-sizex", ".rubik.sizex", XrmoptionSepArg, 0},
        {"-sizey", ".rubik.sizey", XrmoptionSepArg, 0},
        {"-sizez", ".rubik.sizez", XrmoptionSepArg, 0},
	{"-hideshuffling", ".rubik.hideshuffling", XrmoptionNoArg, "on"},
	{"+hideshuffling", ".rubik.hideshuffling", XrmoptionNoArg, "off"}
};

static argtype vars[] =
{
	{&sizex, "sizex", "SizeX", DEF_SIZEX, t_Int},
	{&sizey, "sizey", "SizeY", DEF_SIZEY, t_Int},
	{&sizez, "sizez", "SizeZ", DEF_SIZEZ, t_Int},
	{&hideshuffling, "hideshuffling", "Hideshuffling", DEF_HIDESHUFFLING, t_Bool}
};

static OptionStruct desc[] =
{
	{"-sizex num", "number of cubies along x axis (overrides size)"},
	{"-sizey num", "number of cubies along y axis (overrides size)"},
	{"-sizez num", "number of cubies along z axis (overrides size)"},
	{"-/+hideshuffling", "turn on/off hidden shuffle phase"}
};

ModeSpecOpt rubik_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   rubik_description =
{"rubik", "init_rubik", "draw_rubik", "release_rubik",
 "draw_rubik", "change_rubik", (char *) NULL, &rubik_opts,
 10000, -30, 5, -6, 64, 1.0, "",
 "Shows an auto-solving Rubik's Cube", 0, NULL};

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

#define MINSIZE 2
#define MAXSIZEX (rp->sizex)
#define MAXSIZEY (rp->sizey)
#define MAXSIZEZ (rp->sizez)
#define AVSIZE ((rp->sizex+rp->sizey+rp->sizez)/3.0)     /* Use of this should be minimized */
#define MAXMAXSIZE (MAX(MAXSIZEX,MAX(MAXSIZEY,MAXSIZEZ)))
#define MAXSIZEXY (MAXSIZEX*MAXSIZEY)
#define MAXSIZEYZ (MAXSIZEY*MAXSIZEZ)
#define MAXSIZEZX (MAXSIZEZ*MAXSIZEX)
#define LASTX (MAXSIZEX-1)
#define LASTY (MAXSIZEY-1)
#define LASTZ (MAXSIZEZ-1)
/* These are not likely to change but... */
#define FIRSTX 0
#define FIRSTY 0
#define FIRSTZ 0

#define Scale4Window               (0.9/AVSIZE)
#define Scale4Iconic               (2.1/AVSIZE)

#define MAXORIENT 4		/* Number of orientations of a square */
#define MAXFACES 6		/* Number of faces */

/* Directions relative to the face of a cubie */
#define TOP 0
#define RIGHT 1
#define BOTTOM 2
#define LEFT 3
#define CW (MAXORIENT+1)
#define HALF (MAXORIENT+2)
#define CCW (2*MAXORIENT-1)

#define TOP_FACE 0
#define LEFT_FACE 1
#define FRONT_FACE 2
#define RIGHT_FACE 3
#define BOTTOM_FACE 4
#define BACK_FACE 5
#define NO_FACE (MAXFACES)
#define NO_ROTATION (2*MAXORIENT)
#define NO_DEPTH MAXMAXSIZE

#define REVX(a) (MAXSIZEX - a - 1)
#define REVY(a) (MAXSIZEY - a - 1)
#define REVZ(a) (MAXSIZEZ - a - 1)

#define CUBELEN 0.50
#define CUBEROUND (CUBELEN-0.05)
#define STICKERLONG (CUBEROUND-0.05)
#define STICKERSHORT (STICKERLONG-0.05)
#define STICKERDEPTH (CUBELEN+0.01)

#define ObjCubit        0
#define MaxObj          1
typedef struct _RubikLoc {
	int         face;
	int         rotation;	/* Not used yet */
} RubikLoc;

typedef struct _RubikRowNext {
	int         face, direction, sideFace;
} RubikRowNext;

typedef struct _RubikMove {
	int         face, direction;
	int         position;
} RubikMove;

typedef struct _RubikSlice {
	int         face, rotation;
	int         depth;
} RubikSlice;

/*-
 * Pick a face and a direction on face the next face and orientation
 * is then known.
 */
static RubikLoc slideNextRow[MAXFACES][MAXORIENT] =
{
	{
		{5, TOP},
		{3, RIGHT},
		{2, TOP},
		{1, LEFT}},
	{
		{0, RIGHT},
		{2, TOP},
		{4, LEFT},
		{5, BOTTOM}},
	{
		{0, TOP},
		{3, TOP},
		{4, TOP},
		{1, TOP}},
	{
		{0, LEFT},
		{5, BOTTOM},
		{4, RIGHT},
		{2, TOP}},
	{
		{2, TOP},
		{3, LEFT},
		{5, TOP},
		{1, RIGHT}},
	{
		{4, TOP},
		{3, BOTTOM},
		{0, TOP},
		{1, BOTTOM}}
};

/*-
 * Examine cubie 0 on each face, its 4 movements (well only 2 since the
 * other 2 will be opposites) and translate it into slice movements).
 * CW = DEEP Depth CCW == SHALLOW Depth with reference to faces 0, 1, and 2
 */
static RubikLoc rotateSlice[MAXFACES][MAXORIENT / 2] =
{
	{
		{1, CCW},
		{2, CW},
	},
	{
		{2, CW},
		{0, CCW},
	},
	{
		{1, CCW},
		{0, CCW},
	},
	{
		{2, CCW},
		{0, CCW},
	},
	{
		{1, CCW},
		{2, CCW},
	},
	{
		{1, CCW},
		{0, CW},
	}
};

/*-
 * Rotate face clockwise by a number of orients, then the top of the
 * face then points to this face
 */
static int  rowToRotate[MAXFACES][MAXORIENT] =
{
	{3, 2, 1, 5},
	{2, 4, 5, 0},
	{3, 4, 1, 0},
	{5, 4, 2, 0},
	{3, 5, 1, 2},
	{3, 0, 1, 4}
};

/*
 * This translates a clockwise move to something more manageable
 */
static RubikRowNext rotateToRow[MAXFACES] =	/*CW to min face */
{
	{1, LEFT, TOP},
	{0, BOTTOM, RIGHT},
	{0, RIGHT, BOTTOM},
	{0, TOP, LEFT},
	{1, RIGHT, BOTTOM},
	{0, LEFT, TOP}
};

typedef struct {
	GLint       WindH, WindW;
	GLfloat     step;
	RubikMove  *moves;
	int         storedmoves;
	int         degreeTurn;
	int         shufflingmoves;
	int         sizex, sizey, sizez;
	float       avsize, avsizeSq;
	int         action;
	int         done;
	GLfloat     anglestep;
	RubikLoc   *cubeLoc[MAXFACES];
	RubikLoc   *rowLoc[MAXORIENT];
	RubikMove   movement;
	GLfloat     rotatestep;
	GLfloat     PX, PY, VX, VY;
	GLXContext *glx_context;
} rubikstruct;

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
static float MaterialGreen[] =
{0.0, 0.5, 0.0, 1.0};
static float MaterialBlue[] =
{0.0, 0.0, 0.5, 1.0};
static float MaterialYellow[] =
{0.7, 0.7, 0.0, 1.0};
static float MaterialOrange[] =
{0.9, 0.45, 0.36, 1.0};

#if 0
static float MaterialMagenta[] =
{0.7, 0.0, 0.7, 1.0};
static float MaterialCyan[] =
{0.0, 0.7, 0.7, 1.0};

#endif
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

static rubikstruct *rubik = (rubikstruct *) NULL;


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
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray6);
			else
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialYellow);
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
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialOrange);
			break;
		case BACK_FACE:
			if (mono)
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray5);
			else
				glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialBlue);
			break;
#if 0
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialCyan);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialMagenta);
#endif
	}
}

static void
faceSizes(rubikstruct * rp, int face, int * sizeOfRow, int * sizeOfColumn)
{
	switch (face) {
		case 0: /* TOP */
		case 4: /* BOTTOM */
			*sizeOfRow = MAXSIZEX;
			*sizeOfColumn = MAXSIZEZ;
			break;
		case 1: /* LEFT */
		case 3: /* RIGHT */
			*sizeOfRow = MAXSIZEZ;
			*sizeOfColumn = MAXSIZEY;
			break;
		case 2: /* FRONT */
		case 5: /* BACK */
			*sizeOfRow = MAXSIZEX;
			*sizeOfColumn = MAXSIZEY;
			break;
	}
}

static Bool
checkFaceSquare(rubikstruct * rp, int face)
{
	int sizeOfRow, sizeOfColumn;

	faceSizes(rp, face, &sizeOfRow, &sizeOfColumn);
	return (sizeOfRow == sizeOfColumn);
	/* Cubes can be made square with a 4x2 face where 90 degree turns
	 * should be permitted but that is kind of complicated for me.
	 * This can be done in 2 ways where the side of the cubies are
	 * the same size and one where one side (the side with half the
	 * number of cubies) is twice the size of the other.  The first is
	 * complicated because faces of cubies can go under other faces.
	 * The second way is similar to "banded cubes" where scotch tape
	 * restricts the moves of some cubes.  Here you have to keep track
	 * of the restrictions and show banded cubies graphically as one
	 * cube.
	 */
}

static int
sizeFace(rubikstruct * rp, int face)
{
	int sizeOfRow, sizeOfColumn;

	faceSizes(rp, face, &sizeOfRow, &sizeOfColumn);
	return (sizeOfRow * sizeOfColumn);
}

static int
sizeRow(rubikstruct * rp, int face)
{
	int sizeOfRow, sizeOfColumn;  /* sizeOfColumn not used */

	faceSizes(rp, face, &sizeOfRow, &sizeOfColumn);
	return sizeOfRow;
}

static Bool
draw_stickerless_cubit(rubikstruct *rp)
{
	glBegin(GL_QUADS);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
	/* Put sticker here */
	glNormal3f(0.00, 0.00, 1.00);
	glVertex3f(-CUBEROUND, -CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, -CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(-CUBEROUND, CUBEROUND, CUBELEN);
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

	/* Edges of cubit */
	glNormal3f(-1.00, -1.00, 0.00);
	glVertex3f(-CUBEROUND, -CUBELEN, -CUBEROUND);
	glVertex3f(-CUBEROUND, -CUBELEN, CUBEROUND);
	glVertex3f(-CUBELEN, -CUBEROUND, CUBEROUND);
	glVertex3f(-CUBELEN, -CUBEROUND, -CUBEROUND);
	glNormal3f(1.00, 1.00, 0.00);
	glVertex3f(CUBEROUND, CUBELEN, -CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, -CUBEROUND);
	glNormal3f(-1.00, 1.00, 0.00);
	glVertex3f(-CUBELEN, CUBEROUND, -CUBEROUND);
	glVertex3f(-CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(-CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(-CUBEROUND, CUBELEN, -CUBEROUND);
	glNormal3f(1.00, -1.00, 0.00);
	glVertex3f(CUBELEN, -CUBEROUND, -CUBEROUND);
	glVertex3f(CUBELEN, -CUBEROUND, CUBEROUND);
	glVertex3f(CUBEROUND, -CUBELEN, CUBEROUND);
	glVertex3f(CUBEROUND, -CUBELEN, -CUBEROUND);
	glNormal3f(0.00, -1.00, -1.00);
	glVertex3f(-CUBEROUND, -CUBEROUND, -CUBELEN);
	glVertex3f(CUBEROUND, -CUBEROUND, -CUBELEN);
	glVertex3f(CUBEROUND, -CUBELEN, -CUBEROUND);
	glVertex3f(-CUBEROUND, -CUBELEN, -CUBEROUND);
	glNormal3f(0.00, 1.00, 1.00);
	glVertex3f(-CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(-CUBEROUND, CUBELEN, CUBEROUND);
	glNormal3f(0.00, -1.00, 1.00);
	glVertex3f(-CUBEROUND, -CUBELEN, CUBEROUND);
	glVertex3f(CUBEROUND, -CUBELEN, CUBEROUND);
	glVertex3f(CUBEROUND, -CUBEROUND, CUBELEN);
	glVertex3f(-CUBEROUND, -CUBEROUND, CUBELEN);
	glNormal3f(0.00, 1.00, -1.00);
	glVertex3f(-CUBEROUND, CUBELEN, -CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, -CUBEROUND);
	glVertex3f(CUBEROUND, CUBEROUND, -CUBELEN);
	glVertex3f(-CUBEROUND, CUBEROUND, -CUBELEN);
	glNormal3f(-1.00, 0.00, -1.00);
	glVertex3f(-CUBELEN, -CUBEROUND, -CUBEROUND);
	glVertex3f(-CUBELEN, CUBEROUND, -CUBEROUND);
	glVertex3f(-CUBEROUND, CUBEROUND, -CUBELEN);
	glVertex3f(-CUBEROUND, -CUBEROUND, -CUBELEN);
	glNormal3f(1.00, 0.00, 1.00);
	glVertex3f(CUBELEN, -CUBEROUND, CUBEROUND);
	glVertex3f(CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, -CUBEROUND, CUBELEN);
	glNormal3f(1.00, 0.00, -1.00);
	glVertex3f(CUBEROUND, -CUBEROUND, -CUBELEN);
	glVertex3f(CUBEROUND, CUBEROUND, -CUBELEN);
	glVertex3f(CUBELEN, CUBEROUND, -CUBEROUND);
	glVertex3f(CUBELEN, -CUBEROUND, -CUBEROUND);
	glNormal3f(-1.00, 0.00, 1.00);
	glVertex3f(-CUBEROUND, -CUBEROUND, CUBELEN);
	glVertex3f(-CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(-CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(-CUBELEN, -CUBEROUND, CUBEROUND);
	glEnd();
	glBegin(GL_TRIANGLES);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
	/* Corners of cubit */
	glNormal3f(1.00, 1.00, 1.00);
	glVertex3f(CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(CUBELEN, CUBEROUND, CUBEROUND);
	glVertex3f(CUBEROUND, CUBELEN, CUBEROUND);
	glNormal3f(-1.00, -1.00, -1.00);
	glVertex3f(-CUBEROUND, -CUBELEN, -CUBEROUND);
	glVertex3f(-CUBELEN, -CUBEROUND, -CUBEROUND);
	glVertex3f(-CUBEROUND, -CUBEROUND, -CUBELEN);
	glNormal3f(-1.00, 1.00, 1.00);
	glVertex3f(-CUBEROUND, CUBEROUND, CUBELEN);
	glVertex3f(-CUBEROUND, CUBELEN, CUBEROUND);
	glVertex3f(-CUBELEN, CUBEROUND, CUBEROUND);
	glNormal3f(1.00, -1.00, -1.00);
	glVertex3f(CUBELEN, -CUBEROUND, -CUBEROUND);
	glVertex3f(CUBEROUND, -CUBELEN, -CUBEROUND);
	glVertex3f(CUBEROUND, -CUBEROUND, -CUBELEN);
	glNormal3f(1.00, -1.00, 1.00);
	glVertex3f(CUBEROUND, -CUBEROUND, CUBELEN);
	glVertex3f(CUBEROUND, -CUBELEN, CUBEROUND);
	glVertex3f(CUBELEN, -CUBEROUND, CUBEROUND);
	glNormal3f(-1.00, 1.00, -1.00);
	glVertex3f(-CUBELEN, CUBEROUND, -CUBEROUND);
	glVertex3f(-CUBEROUND, CUBELEN, -CUBEROUND);
	glVertex3f(-CUBEROUND, CUBEROUND, -CUBELEN);
	glNormal3f(-1.00, -1.00, 1.00);
	glVertex3f(-CUBEROUND, -CUBEROUND, CUBELEN);
	glVertex3f(-CUBELEN, -CUBEROUND, CUBEROUND);
	glVertex3f(-CUBEROUND, -CUBELEN, CUBEROUND);
	glNormal3f(1.00, 1.00, -1.00);
	glVertex3f(CUBELEN, CUBEROUND, -CUBEROUND);
	glVertex3f(CUBEROUND, CUBEROUND, -CUBELEN);
	glVertex3f(CUBEROUND, CUBELEN, -CUBEROUND);
	glEnd();
	return True;
}

static Bool
draw_cubit(ModeInfo * mi,
	   int back, int front, int left, int right, int bottom, int top)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];
	int         mono = MI_IS_MONO(mi);

	if (!draw_stickerless_cubit(rp))
		return False;
	if (back != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(back, mono);
		glNormal3f(0.00, 0.00, -1.00);
		glVertex3f(-STICKERSHORT, STICKERLONG, -STICKERDEPTH);
		glVertex3f(STICKERSHORT, STICKERLONG, -STICKERDEPTH);
		glVertex3f(STICKERLONG, STICKERSHORT, -STICKERDEPTH);
		glVertex3f(STICKERLONG, -STICKERSHORT, -STICKERDEPTH);
		glVertex3f(STICKERSHORT, -STICKERLONG, -STICKERDEPTH);
		glVertex3f(-STICKERSHORT, -STICKERLONG, -STICKERDEPTH);
		glVertex3f(-STICKERLONG, -STICKERSHORT, -STICKERDEPTH);
		glVertex3f(-STICKERLONG, STICKERSHORT, -STICKERDEPTH);
		glEnd();
	}
	if (front != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(front, mono);
		glNormal3f(0.00, 0.00, 1.00);
		glVertex3f(-STICKERSHORT, -STICKERLONG, STICKERDEPTH);
		glVertex3f(STICKERSHORT, -STICKERLONG, STICKERDEPTH);
		glVertex3f(STICKERLONG, -STICKERSHORT, STICKERDEPTH);
		glVertex3f(STICKERLONG, STICKERSHORT, STICKERDEPTH);
		glVertex3f(STICKERSHORT, STICKERLONG, STICKERDEPTH);
		glVertex3f(-STICKERSHORT, STICKERLONG, STICKERDEPTH);
		glVertex3f(-STICKERLONG, STICKERSHORT, STICKERDEPTH);
		glVertex3f(-STICKERLONG, -STICKERSHORT, STICKERDEPTH);
		glEnd();
	}
	if (left != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(left, mono);
		glNormal3f(-1.00, 0.00, 0.00);
		glVertex3f(-STICKERDEPTH, -STICKERSHORT, STICKERLONG);
		glVertex3f(-STICKERDEPTH, STICKERSHORT, STICKERLONG);
		glVertex3f(-STICKERDEPTH, STICKERLONG, STICKERSHORT);
		glVertex3f(-STICKERDEPTH, STICKERLONG, -STICKERSHORT);
		glVertex3f(-STICKERDEPTH, STICKERSHORT, -STICKERLONG);
		glVertex3f(-STICKERDEPTH, -STICKERSHORT, -STICKERLONG);
		glVertex3f(-STICKERDEPTH, -STICKERLONG, -STICKERSHORT);
		glVertex3f(-STICKERDEPTH, -STICKERLONG, STICKERSHORT);
		glEnd();
	}
	if (right != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(right, mono);
		glNormal3f(1.00, 0.00, 0.00);
		glVertex3f(STICKERDEPTH, -STICKERSHORT, -STICKERLONG);
		glVertex3f(STICKERDEPTH, STICKERSHORT, -STICKERLONG);
		glVertex3f(STICKERDEPTH, STICKERLONG, -STICKERSHORT);
		glVertex3f(STICKERDEPTH, STICKERLONG, STICKERSHORT);
		glVertex3f(STICKERDEPTH, STICKERSHORT, STICKERLONG);
		glVertex3f(STICKERDEPTH, -STICKERSHORT, STICKERLONG);
		glVertex3f(STICKERDEPTH, -STICKERLONG, STICKERSHORT);
		glVertex3f(STICKERDEPTH, -STICKERLONG, -STICKERSHORT);
		glEnd();
	}
	if (bottom != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(bottom, mono);
		glNormal3f(0.00, -1.00, 0.00);
		glVertex3f(STICKERLONG, -STICKERDEPTH, -STICKERSHORT);
		glVertex3f(STICKERLONG, -STICKERDEPTH, STICKERSHORT);
		glVertex3f(STICKERSHORT, -STICKERDEPTH, STICKERLONG);
		glVertex3f(-STICKERSHORT, -STICKERDEPTH, STICKERLONG);
		glVertex3f(-STICKERLONG, -STICKERDEPTH, STICKERSHORT);
		glVertex3f(-STICKERLONG, -STICKERDEPTH, -STICKERSHORT);
		glVertex3f(-STICKERSHORT, -STICKERDEPTH, -STICKERLONG);
		glVertex3f(STICKERSHORT, -STICKERDEPTH, -STICKERLONG);
		glEnd();
	}
	if (top != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(top, mono);
		glNormal3f(0.00, 1.00, 0.00);
		glVertex3f(-STICKERLONG, STICKERDEPTH, -STICKERSHORT);
		glVertex3f(-STICKERLONG, STICKERDEPTH, STICKERSHORT);
		glVertex3f(-STICKERSHORT, STICKERDEPTH, STICKERLONG);
		glVertex3f(STICKERSHORT, STICKERDEPTH, STICKERLONG);
		glVertex3f(STICKERLONG, STICKERDEPTH, STICKERSHORT);
		glVertex3f(STICKERLONG, STICKERDEPTH, -STICKERSHORT);
		glVertex3f(STICKERSHORT, STICKERDEPTH, -STICKERLONG);
		glVertex3f(-STICKERSHORT, STICKERDEPTH, -STICKERLONG);
		glEnd();
	}
	return True;
}

/* Convert move to weird general notation */
static void
convertMove(rubikstruct * rp, RubikMove move, RubikSlice * slice)
{
	RubikLoc    plane;
	int         sizeOfRow, sizeOfColumn;

	plane = rotateSlice[(int) move.face][move.direction % 2];
	(*slice).face = plane.face;
	(*slice).rotation = plane.rotation;

	faceSizes(rp, move.face, &sizeOfRow, &sizeOfColumn);
	if (plane.face == 1 || /* VERTICAL */
	    (plane.face == 2 && (move.face == 1 || move.face == 3))) {
		if ((*slice).rotation == CW)
			(*slice).depth = sizeOfRow - 1 - move.position %
				sizeOfRow;
		else
			(*slice).depth = move.position % sizeOfRow;
	} else { /* (plane.face == 0 ||  *//* HORIZONTAL *//*
		(plane.face == 2 && (move.face == 0 || move.face == 4))) */
		if ((*slice).rotation == CW)
			(*slice).depth = sizeOfColumn - 1 - move.position /
				sizeOfRow;
		else
			(*slice).depth = move.position / sizeOfRow;
	}
	/* If (*slice).depth = 0 then face 0, face 1, or face 2 moves */
	if (move.direction / 2)
		(*slice).rotation = ((*slice).rotation == CW) ? CCW : CW;
}

/* Assume the size is at least 2, or its just not challenging... */
static Bool
draw_cube(ModeInfo * mi)
{
#define S1 1
#define SX ((GLint)S1*(MAXSIZEX-1))
#define SY ((GLint)S1*(MAXSIZEY-1))
#define SZ ((GLint)S1*(MAXSIZEZ-1))
#define HALFX (((GLfloat)MAXSIZEX-1.0)/2.0)
#define HALFY (((GLfloat)MAXSIZEY-1.0)/2.0)
#define HALFZ (((GLfloat)MAXSIZEZ-1.0)/2.0)
#define MIDX(a) (((GLfloat)(2*a-MAXSIZEX+1))/2.0)
#define MIDY(a) (((GLfloat)(2*a-MAXSIZEY+1))/2.0)
#define MIDZ(a) (((GLfloat)(2*a-MAXSIZEZ+1))/2.0)
#define DRAW_CUBIT(mi,b,f,l,r,bm,t) if (!draw_cubit(mi,b,f,l,r,bm,t)) return False
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];
	RubikSlice  slice;
	GLfloat     rotatestep;
	int         i, j, k;

	if (rp->movement.face == NO_FACE) {
		slice.face = NO_FACE;
		slice.rotation = NO_ROTATION;
		slice.depth = NO_DEPTH;
	} else {
		convertMove(rp, rp->movement, &slice);
	}
	rotatestep = (slice.rotation == CCW) ? rp->rotatestep : -rp->rotatestep;


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
			if (slice.depth == MAXSIZEY - 1)
				glRotatef(rotatestep, 0, HALFY, 0);

			glTranslatef(-HALFX, -HALFY, -HALFZ);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, 0, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * LASTY].face, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
			}
			glTranslatef(0, 0, S1);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * LASTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, -SZ);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * FIRSTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * LASTZ].face, NO_FACE);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, 0, S1);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * REVZ(k)].face, NO_FACE);
				}
				glTranslatef(0, 0, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * LASTY].face,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * FIRSTZ].face, NO_FACE);
			}
			glTranslatef(S1, 0, -SZ);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, 0, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * LASTY].face,
					   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
			}
			glTranslatef(0, 0, S1);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * LASTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			glPopMatrix();
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glPushMatrix();
				if (slice.depth == REVY(j))
					glRotatef(rotatestep, 0, HALFY, 0);
				glTranslatef(-HALFX, MIDY(j), -HALFZ);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * j].face, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, 0, S1);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * REVY(j)].face, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, 0, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * REVY(j)].face,
					   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(1, 0, -SZ);
					DRAW_CUBIT(mi,
						   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * j].face, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
					/* Center */
					glTranslatef(0, 0, SZ);
					DRAW_CUBIT(mi,
						   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * REVY(j)].face,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(S1, 0, -SZ);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * j].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, 0, S1);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * REVY(j)].face,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, 0, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * REVY(j)].face,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
				glPopMatrix();
			}
			if (slice.depth == 0)
				glRotatef(rotatestep, 0, HALFY, 0);

			glTranslatef(-HALFX, HALFY, -HALFZ);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * LASTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, 0, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * FIRSTY].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * k].face);
			}
			glTranslatef(0, 0, S1);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * FIRSTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * LASTZ].face);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, -SZ);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * LASTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * FIRSTZ].face);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, 0, S1);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * k].face);
				}
				glTranslatef(0, 0, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * FIRSTY].face,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * LASTZ].face);
			}
			glTranslatef(S1, 0, -SZ);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * LASTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * FIRSTZ].face);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, 0, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * FIRSTY].face,
					   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * k].face);
			}
			glTranslatef(0, 0, S1);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * LASTZ].face);
			break;
		case LEFT_FACE:	/* RIGHT_FACE too */
			/* rotatestep is negative because the RIGHT face is the default here */
			glPushMatrix();
			if (slice.depth == 0)
				glRotatef(-rotatestep, HALFX, 0, 0);

			glTranslatef(-HALFX, -HALFY, -HALFZ);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(0, S1, 0);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * j].face, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(0, S1, 0);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * LASTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, -SY, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * LASTY].face, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(0, S1, 0);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * REVY(j)].face, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, S1, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * FIRSTY].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * k].face);
			}
			glTranslatef(0, -SY, S1);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * LASTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(0, S1, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * REVY(j)].face,
					   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(0, S1, 0);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * FIRSTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * LASTZ].face);
			glPopMatrix();
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glPushMatrix();
				if (slice.depth == i)
					glRotatef(-rotatestep, HALFX, 0, 0);
				glTranslatef(MIDX(i), -HALFY, -HALFZ);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * FIRSTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * LASTZ].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(0, S1, 0);
					DRAW_CUBIT(mi,
						   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * j].face, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, S1, 0);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * LASTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * FIRSTZ].face);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, -SY, S1);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * REVZ(k)].face, NO_FACE);
					/* Center */
					glTranslatef(0, SY, 0);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * k].face);
				}
				glTranslatef(0, -SY, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * LASTY].face,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * FIRSTZ].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(0, S1, 0);
					DRAW_CUBIT(mi,
						   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * REVY(j)].face,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, S1, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * FIRSTY].face,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * LASTZ].face);
				glPopMatrix();
			}
			if (slice.depth == MAXSIZEX - 1)
				glRotatef(-rotatestep, HALFX, 0, 0);
			glTranslatef(HALFX, -HALFY, -HALFZ);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(0, S1, 0);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * j].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(0, S1, 0);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * LASTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * FIRSTZ].face);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, -SY, S1);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * LASTY].face,
					   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(0, S1, 0);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * REVY(j)].face,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, S1, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * FIRSTY].face,
					   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * k].face);
			}
			glTranslatef(0, -SY, S1);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * LASTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(0, S1, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * REVY(j)].face,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(0, S1, 0);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * LASTZ].face);
			break;
		case FRONT_FACE:	/* BACK_FACE too */
			glPushMatrix();
			if (slice.depth == MAXSIZEZ - 1)
				glRotatef(rotatestep, 0, 0, HALFZ);

			glTranslatef(-HALFX, -HALFY, -HALFZ);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, 0);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * FIRSTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * LASTZ].face, NO_FACE);
			}
			glTranslatef(S1, 0, 0);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(-SX, S1, 0);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * j].face, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(S1, 0, 0);
					DRAW_CUBIT(mi,
						   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * j].face, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(S1, 0, 0);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * j].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(-SX, S1, 0);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * LASTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, 0);
				DRAW_CUBIT(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * LASTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * FIRSTZ].face);
			}
			glTranslatef(S1, 0, 0);
			DRAW_CUBIT(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * LASTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * FIRSTZ].face);
			glPopMatrix();
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glPushMatrix();
				if (slice.depth == REVZ(k))
					glRotatef(rotatestep, 0, 0, HALFZ);
				glTranslatef(-HALFX, -HALFY, MIDZ(k));
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * LASTY].face, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(S1, 0, 0);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * REVZ(k)].face, NO_FACE);
				}
				glTranslatef(S1, 0, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * LASTY].face,
					   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(-SX, S1, 0);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * REVY(j)].face, NO_FACE,
						   NO_FACE, NO_FACE);
					/* Center */
					glTranslatef(SX, 0, 0);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * REVY(j)].face,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(-SX, S1, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * FIRSTY].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * k].face);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(S1, 0, 0);
					DRAW_CUBIT(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * k].face);
				}
				glTranslatef(S1, 0, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * FIRSTY].face,
					   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * k].face);
				glPopMatrix();
			}
			if (slice.depth == 0)
				glRotatef(rotatestep, 0, 0, HALFZ);
			glTranslatef(-HALFX, -HALFY, HALFZ);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * LASTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * LASTY].face,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * FIRSTZ].face, NO_FACE);
			}
			glTranslatef(S1, 0, 0);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * LASTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(-SX, S1, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * REVY(j)].face,
					   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(S1, 0, 0);
					DRAW_CUBIT(mi,
						   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * REVY(j)].face,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(S1, 0, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * REVY(j)].face,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(-SX, S1, 0);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * FIRSTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * LASTZ].face);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, 0);
				DRAW_CUBIT(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * FIRSTY].face,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * LASTZ].face);
			}
			glTranslatef(S1, 0, 0);
			DRAW_CUBIT(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * LASTZ].face);
			break;
	}
	return True;
#undef S1
}

/* From David Bagley's xrubik.  Used by permission. ;)  */
static void
readRC(rubikstruct * rp, int face, int dir, int h, int orient, int size)
{
	int         g, sizeOfRow;

	sizeOfRow = sizeRow(rp, face);
	if (dir == TOP || dir == BOTTOM)
		for (g = 0; g < size; g++)
			rp->rowLoc[orient][g] =
				rp->cubeLoc[face][g * sizeOfRow + h];
	else			/* dir == RIGHT || dir == LEFT */
		for (g = 0; g < size; g++)
			rp->rowLoc[orient][g] =
				rp->cubeLoc[face][h * sizeOfRow + g];
}

static void
rotateRC(rubikstruct * rp, int rotate, int orient, int size)
{
	int         g;

	for (g = 0; g < size; g++)
		rp->rowLoc[orient][g].rotation =
			(rp->rowLoc[orient][g].rotation + rotate) % MAXORIENT;
}

static void
reverseRC(rubikstruct * rp, int orient, int size)
{
	int         g;
	RubikLoc    temp;

	for (g = 0; g < size / 2; g++) {
		temp = rp->rowLoc[orient][size - 1 - g];
		rp->rowLoc[orient][size - 1 - g] = rp->rowLoc[orient][g];
		rp->rowLoc[orient][g] = temp;
	}
}

static void
writeRC(rubikstruct * rp, int face, int dir, int h, int orient, int size)
{
	int         g, position, sizeOfRow;

	sizeOfRow = sizeRow(rp, face);
	if (dir == TOP || dir == BOTTOM) {
		for (g = 0; g < size; g++) {
			position = g * sizeOfRow + h;
			rp->cubeLoc[face][position] = rp->rowLoc[orient][g];
			/* DrawSquare(face, position); */
		}
	} else {		/* dir == RIGHT || dir == LEFT */
		for (g = 0; g < size; g++) {
			position = h * sizeOfRow + g;
			rp->cubeLoc[face][position] = rp->rowLoc[orient][g];
			/* DrawSquare(face, position); */
		}
	}
}

static Bool
rotateFace(rubikstruct * rp, int face, int direction)
{
	int         position, i, j, sizeOfRow, sizeOfColumn, sizeOnPlane;
	RubikLoc   *faceLoc;

	faceSizes(rp, face, &sizeOfRow, &sizeOfColumn);
	sizeOnPlane = sizeOfRow * sizeOfColumn;
	if ((faceLoc = (RubikLoc *) malloc(sizeOnPlane *
			sizeof (RubikLoc))) == NULL) {
		return False;
	}
	/* Read Face */
	for (position = 0; position < sizeOnPlane; position++)
		faceLoc[position] = rp->cubeLoc[face][position];
	/* Write Face */
	for (position = 0; position < sizeOnPlane; position++) {
		i = position % sizeOfRow;
		j = position / sizeOfRow;
		if (direction == CW)
			rp->cubeLoc[face][position] =
				faceLoc[(sizeOfRow - i - 1) * sizeOfRow + j];
		else if (direction == CCW)
			rp->cubeLoc[face][position] =
				faceLoc[i * sizeOfRow + sizeOfColumn - j - 1];
		else /* (direction == HALF) */
			rp->cubeLoc[face][position] =
				faceLoc[sizeOfRow - i - 1 + (sizeOfColumn - j - 1) * sizeOfRow];
		rp->cubeLoc[face][position].rotation =
			(rp->cubeLoc[face][position].rotation +
				direction - MAXORIENT) % MAXORIENT;
		/* DrawSquare(face, position); */
	}
	if (faceLoc != NULL)
		(void) free((void *) faceLoc);
	return True;
}

/* Yeah this is big and ugly */
static void
slideRC(int face, int direction, int h, int sizeOnOppAxis,
	int *newFace, int *newDirection, int *newH,
	int *rotate, Bool *reverse)
{
	*newFace = slideNextRow[face][direction].face;
	*rotate = slideNextRow[face][direction].rotation;
	*newDirection = (*rotate + direction) % MAXORIENT;
	switch (*rotate) {
		case TOP:
			*newH = h;
			*reverse = False;
			break;
		case RIGHT:
			if (*newDirection == TOP || *newDirection == BOTTOM) {
				*newH = sizeOnOppAxis - 1 - h;
				*reverse = False;
			} else {	/* *newDirection == RIGHT || *newDirection == LEFT */
				*newH = h;
				*reverse = True;
			}
				break;
		case BOTTOM:
			*newH = sizeOnOppAxis - 1 - h;
			*reverse = True;
			break;
		case LEFT:
			if (*newDirection == TOP || *newDirection == BOTTOM) {
				*newH = h;
				*reverse = True;
			} else {	/* *newDirection == RIGHT || *newDirection == LEFT */
				*newH = sizeOnOppAxis - 1 - h;
				*reverse = False;
			}
			break;
		default:
			(void) printf("slideRC: rotate %d\n", *rotate);
			*newH = 0;
			*reverse = False;
	}
}

static Bool
moveRubik(rubikstruct * rp, int face, int direction, int position)
{
	int         newFace, newDirection, rotate, reverse; 
	int         h, k, newH;
	int         i, j, sizeOfRow, sizeOfColumn, sizeOnAxis, sizeOnOppAxis;

	faceSizes(rp, face, &sizeOfRow, &sizeOfColumn);
	if (direction == CW || direction == CCW) {
		direction = (direction == CCW) ?
			(rotateToRow[face].direction + 2) % MAXORIENT :
			rotateToRow[face].direction;
		if (rotateToRow[face].sideFace == RIGHT) {
			i = j = sizeOfColumn - 1;
		} else if (rotateToRow[face].sideFace == BOTTOM) {
			i = j = sizeOfRow - 1;
		} else {
			i = j = 0;
		}
		face = rotateToRow[face].face;
		position = j * sizeOfRow + i;
	}
	i = position % sizeOfRow;
	j = position / sizeOfRow;
	h = (direction == TOP || direction == BOTTOM) ? i : j;
	if (direction == TOP || direction == BOTTOM) {
		sizeOnAxis = sizeOfColumn;
		sizeOnOppAxis = sizeOfRow;
	} else {
		sizeOnAxis = sizeOfRow;
		sizeOnOppAxis = sizeOfColumn;
	}
	/* rotate sides CW or CCW or HALF) */

	if (h == sizeOnOppAxis - 1) {
		newDirection = (direction == TOP || direction == BOTTOM) ?
			TOP : RIGHT;
		if (rp->degreeTurn == 180) {
			if (!rotateFace(rp, rowToRotate[face][newDirection], HALF))
				return False;
		} else if (direction == TOP || direction == RIGHT) {
			if (!rotateFace(rp, rowToRotate[face][newDirection], CW))
				return False;
		} else {		/* direction == BOTTOM || direction == LEFT */
			if (!rotateFace(rp, rowToRotate[face][newDirection], CCW))
				return False;
		}
	}
	if (h == 0) {
		newDirection = (direction == TOP || direction == BOTTOM) ?
			BOTTOM : LEFT;
		if (rp->degreeTurn == 180) {
			if (!rotateFace(rp, rowToRotate[face][newDirection], HALF))
				return False;
		} else if (direction == TOP || direction == RIGHT) {
			if (!rotateFace(rp, rowToRotate[face][newDirection], CCW))
				return False;
		} else {		/* direction == BOTTOM  || direction == LEFT */
			if (!rotateFace(rp, rowToRotate[face][newDirection], CW))
				return False;
		}
	}
	/* Slide rows or columns */
	readRC(rp, face, direction, h, 0, sizeOnAxis);
	if (rp->degreeTurn == 180) {
		int sizeOnDepthAxis;

		slideRC(face, direction, h, sizeOnOppAxis,
			&newFace, &newDirection, &newH, &rotate, &reverse);
		sizeOnDepthAxis = sizeFace(rp, newFace) / sizeOnOppAxis;
		readRC(rp, newFace, newDirection, newH, 1, sizeOnDepthAxis);
		rotateRC(rp, rotate, 0, sizeOnAxis);
		if (reverse == True)
			reverseRC(rp, 0, sizeOnAxis);
		face = newFace;
		direction = newDirection;
		h = newH;
		for (k = 2; k <= MAXORIENT + 1; k++) {
			slideRC(face, direction, h, sizeOnOppAxis,
				&newFace, &newDirection, &newH, &rotate, &reverse);
			if (k != MAXORIENT && k != MAXORIENT + 1)
				readRC(rp, newFace, newDirection, newH, k,
					(k % 2) ? sizeOnDepthAxis : sizeOnAxis);
			rotateRC(rp, rotate, k - 2,
					(k % 2) ? sizeOnDepthAxis : sizeOnAxis);
			if (k != MAXORIENT + 1)
				rotateRC(rp, rotate, k - 1,
					(k % 2) ? sizeOnAxis : sizeOnDepthAxis);
			if (reverse == True) {
				reverseRC(rp, k - 2,
					(k % 2) ? sizeOnDepthAxis : sizeOnAxis);
				if (k != MAXORIENT + 1)
					reverseRC(rp, k - 1,
						(k % 2) ? sizeOnAxis : sizeOnDepthAxis);
			}
			writeRC(rp, newFace, newDirection, newH, k - 2,
				(k % 2) ? sizeOnDepthAxis : sizeOnAxis);
			face = newFace;
			direction = newDirection;
			h = newH;
		}
	} else {
		for (k = 1; k <= MAXORIENT; k++) {
			slideRC(face, direction, h, sizeOnOppAxis,
				&newFace, &newDirection, &newH, &rotate, &reverse);
			if (k != MAXORIENT)
				readRC(rp, newFace, newDirection, newH, k, sizeOnAxis);
			rotateRC(rp, rotate, k - 1, sizeOnAxis);
			if (reverse == True)
				reverseRC(rp, k - 1, sizeOnAxis);
			writeRC(rp, newFace, newDirection, newH, k - 1, sizeOnAxis);
			face = newFace;
			direction = newDirection;
			h = newH;
		}
	}	
	return True;
}

#ifdef DEBUG
void
printCube(rubikstruct * rp)
{
	int         face, position, sizeOfRow, sizeOfColumn;

	for (face = 0; face < MAXFACES; face++) {
		faceSizes(rp, face, &sizeOfRow, &sizeOfColumn);
		for (position = 0; position < sizeOfRow * sizeOfColumn; position++) {
			(void) printf("%d %d  ", rp->cubeLoc[face][position].face,
				      rp->cubeLoc[face][position].rotation);
			if (!((position + 1) % sizeOfRow))
				(void) printf("\n");
		}
		(void) printf("\n");
	}
	(void) printf("\n");
}

#endif

static Bool
evalmovement(ModeInfo * mi, RubikMove movement)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];

#ifdef DEBUG
	printCube(rp);
#endif
	if (movement.face < 0 || movement.face >= MAXFACES)
		return True;
	if (!moveRubik(rp, movement.face, movement.direction, movement.position))
		return False;
	return True;
}

static      Bool
compare_moves(rubikstruct * rp, RubikMove move1, RubikMove move2, Bool opp)
{
	RubikSlice  slice1, slice2;

	convertMove(rp, move1, &slice1);
	convertMove(rp, move2, &slice2);
	if (slice1.face == slice2.face &&
	    slice1.depth == slice2.depth) {
		if (slice1.rotation == slice2.rotation) {	/* CW or CCW */
			if (!opp)
				return True;
		} else {
			if (opp)
				return True;
		}
	}
	return False;
}

static Bool
shuffle(ModeInfo * mi)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];
	int         i, face, position;
	RubikMove   move;

	if (sizex)
		i = sizex;
	else
		i = MI_SIZE(mi);
	if (i < -MINSIZE)
		i = NRAND(-i - MINSIZE + 1) + MINSIZE;
	else if (i < MINSIZE)
		i = MINSIZE;

	if (LRAND() % 2 && !sizey && !sizez) { /* Make normal (NxNxN) cubes more likely */
		MAXSIZEX = MAXSIZEY = MAXSIZEZ = i;
	} else {
		MAXSIZEX = i;
		if (sizey)
			i = sizey;
		else
			i = MI_SIZE(mi);
		if (i < -MINSIZE)
			i = NRAND(-i - MINSIZE + 1) + MINSIZE;
		else if (i < MINSIZE)
			i = MINSIZE;
		if (LRAND() % 2 && !sizez) { /* Make more MxNxN more likely than LxMxN */
			MAXSIZEY = MAXSIZEZ = i;
		} else {
			MAXSIZEY = i;
			if (sizez)
				i = sizez;
			else
				i = MI_SIZE(mi);
			if (i < -MINSIZE)
				i = NRAND(-i - MINSIZE + 1) + MINSIZE;
			else if (i < MINSIZE)
				i = MINSIZE;
			MAXSIZEZ = i;
		}
	}

	for (face = 0; face < MAXFACES; face++) {
		if (rp->cubeLoc[face] != NULL)
			(void) free((void *) rp->cubeLoc[face]);
		if ((rp->cubeLoc[face] = (RubikLoc *) malloc(sizeFace(rp, face) *
				sizeof (RubikLoc))) == NULL) {
			return False;
		}
		for (position = 0; position < sizeFace(rp, face); position++) {
			rp->cubeLoc[face][position].face = face;
			rp->cubeLoc[face][position].rotation = TOP;
		}
	}
	for (i = 0; i < MAXORIENT; i++) {
		if (rp->rowLoc[i] != NULL)
			(void) free((void *) rp->rowLoc[i]);
		/* The following is reused so make it the biggest size */
		if ((rp->rowLoc[i] = (RubikLoc *) malloc(MAXMAXSIZE *
				sizeof (RubikLoc))) == NULL) {
			return False;
		}
	}
	rp->storedmoves = MI_COUNT(mi);
	if (rp->storedmoves < 0) {
		if (rp->moves != NULL)
			(void) free((void *) rp->moves);
		rp->moves = (RubikMove *) NULL;
		rp->storedmoves = NRAND(-rp->storedmoves) + 1;
	}
	if ((rp->storedmoves) && (rp->moves == NULL))
		if ((rp->moves = (RubikMove *) calloc(rp->storedmoves + 1,
				 sizeof (RubikMove))) == NULL) {
			return False;
		}
	if (MI_CYCLES(mi) <= 1) {
		rp->anglestep = 90.0;
	} else {
		rp->anglestep = 90.0 / (GLfloat) (MI_CYCLES(mi));
	}

	for (i = 0; i < rp->storedmoves; i++) {
		Bool condition;

		do {
			move.face = NRAND(MAXFACES);
			move.direction = NRAND(MAXORIENT);	/* Exclude CW and CCW, its ok */
			move.position = NRAND(sizeFace(rp, move.face));
			rp->degreeTurn = (checkFaceSquare(rp,
				rowToRotate[move.face][move.direction])) ? 90 : 180;
			condition = True;
			if (i > 0) {	/* avoid immediate undoing moves */
				if (compare_moves(rp, move, rp->moves[i - 1], True))
					condition = False;
				if (rp->degreeTurn == 180 &&
				    compare_moves(rp, move, rp->moves[i - 1], False))
					condition = False;
			}
			if (i > 1)	/* avoid 3 consecutive identical moves */
				if (compare_moves(rp, move, rp->moves[i - 1], False) &&
				    compare_moves(rp, move, rp->moves[i - 2], False))
					condition = False;
			/*
			 * Still some silly moves being made....
			 */
		} while (!condition);
		if (hideshuffling)
			if (!evalmovement(mi, move))
				return False;
		rp->moves[i] = move;
	}
	rp->VX = 0.05;
	if (NRAND(100) < 50)
		rp->VX *= -1;
	rp->VY = 0.05;
	if (NRAND(100) < 50)
		rp->VY *= -1;
	rp->movement.face = NO_FACE;
	rp->rotatestep = 0;
	rp->action = hideshuffling ? ACTION_SOLVE : ACTION_SHUFFLE;
	rp->shufflingmoves = 0;
	rp->done = 0;
	return True;
}

void
reshape(ModeInfo * mi, int width, int height)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];

	glViewport(0, 0, rp->WindW = (GLint) width, rp->WindH = (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 15.0);
	glMatrixMode(GL_MODELVIEW);

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
free_rubik(rubikstruct *rp)
{
	int         i;

	for (i = 0; i < MAXFACES; i++)
		if (rp->cubeLoc[i] != NULL) {
			(void) free((void *) rp->cubeLoc[i]);
			rp->cubeLoc[i] = (RubikLoc *) NULL;
		}
	for (i = 0; i < MAXORIENT; i++)
		if (rp->rowLoc[i] != NULL) {
			(void) free((void *) rp->rowLoc[i]);
			rp->rowLoc[i] = (RubikLoc *) NULL;
		}
	if (rp->moves != NULL) {
		(void) free((void *) rp->moves);
		rp->moves = (RubikMove *) NULL;
	}
}

void
release_rubik(ModeInfo * mi)
{
	if (rubik != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			rubikstruct *rp = &rubik[screen];

			free_rubik(rp);
		}
		(void) free((void *) rubik);
		rubik = (rubikstruct *) NULL;
	}
	FreeAllGL(mi);
}

void
init_rubik(ModeInfo * mi)
{
	rubikstruct *rp;

	if (rubik == NULL) {
		if ((rubik = (rubikstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (rubikstruct))) == NULL)
			return;
	}
	rp = &rubik[MI_SCREEN(mi)];
	rp->step = NRAND(90);
	rp->PX = ((float) LRAND() / (float) MAXRAND) * 2.0 - 1.0;
	rp->PY = ((float) LRAND() / (float) MAXRAND) * 2.0 - 1.0;

	if ((rp->glx_context = init_GL(mi)) != NULL) {

		reshape(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		glDrawBuffer(GL_BACK);
		if (!pinit(mi)) {
			free_rubik(rp);
			if (MI_IS_VERBOSE(mi)) {
				(void) fprintf(stderr,
					"Could not allocate memory for rubik\n");
			}
			return;
		}
	} else {
		MI_CLEARWINDOW(mi);
	}
}

void
draw_rubik(ModeInfo * mi)
{
	Bool bounced = False;
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	rubikstruct *rp;

	if (rubik == NULL)
		return;
	rp = &rubik[MI_SCREEN(mi)];
	if (rp->cubeLoc[0] == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	if (!rp->glx_context)
		return;

	glXMakeCurrent(display, window, *(rp->glx_context));

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	glTranslatef(0.0, 0.0, -10.0);

	rp->PX += rp->VX;
	rp->PY += rp->VY;

	if (rp->PY < -1) {
		rp->PY += (-1) - (rp->PY);
		rp->VY = -rp->VY;
		bounced = True;
	}
	if (rp->PY > 1) {
		rp->PY -= (rp->PY) - 1;
		rp->VY = -rp->VY;
		bounced = True;
	}
	if (rp->PX < -1) {
		rp->PX += (-1) - (rp->PX);
		rp->VX = -rp->VX;
		bounced = True;
	}
	if (rp->PX > 1) {
		rp->PX -= (rp->PX) - 1;
		rp->VX = -rp->VX;
		bounced = True;
	}
	if (bounced) {
		rp->VX += ((float) LRAND() / (float) MAXRAND) * 0.02 - 0.01;
		rp->VX += ((float) LRAND() / (float) MAXRAND) * 0.02 - 0.01;
		if (rp->VX > 0.06)
			rp->VX = 0.06;
		if (rp->VY > 0.06)
			rp->VY = 0.06;
		if (rp->VX < -0.06)
			rp->VX = -0.06;
		if (rp->VY < -0.06)
			rp->VY = -0.06;
	}
	if (!MI_IS_ICONIC(mi)) {
		glTranslatef(rp->PX, rp->PY, 0);
		glScalef(Scale4Window * rp->WindH / rp->WindW, Scale4Window, Scale4Window);
	} else {
		glScalef(Scale4Iconic * rp->WindH / rp->WindW, Scale4Iconic, Scale4Iconic);
	}

	glRotatef(rp->step * 100, 1, 0, 0);
	glRotatef(rp->step * 95, 0, 1, 0);
	glRotatef(rp->step * 90, 0, 0, 1);

	if (!draw_cube(mi)) {
		release_rubik(mi);
		return;
	}
        if (MI_IS_FPS(mi)) do_fps (mi);
	glXSwapBuffers(display, window);

	if (rp->action == ACTION_SHUFFLE) {
		if (rp->done) {
			if (++rp->rotatestep > DELAY_AFTER_SHUFFLING) {
				rp->movement.face = NO_FACE;
				rp->rotatestep = 0;
				rp->action = ACTION_SOLVE;
				rp->done = 0;
			}
		} else {
			if (rp->movement.face == NO_FACE) {
				if (rp->shufflingmoves < rp->storedmoves) {
					rp->rotatestep = 0;
					rp->movement = rp->moves[rp->shufflingmoves];
				} else {
					rp->rotatestep = 0;
					rp->done = 1;
				}
			} else {
				if (rp->rotatestep == 0) {
					if (rp->movement.direction == CW || rp->movement.direction == CCW)
						rp->degreeTurn = (checkFaceSquare(rp, rp->movement.face)) ? 90 : 180;
					else
						rp->degreeTurn = (checkFaceSquare(rp, rowToRotate[rp->movement.face][rp->movement.direction])) ? 90 : 180;
				}
				rp->rotatestep += rp->anglestep;
				if (rp->rotatestep > rp->degreeTurn) {
					if (!evalmovement(mi, rp->movement)) {
						free_rubik(rp);
						if (MI_IS_VERBOSE(mi)) {
							(void) fprintf(stderr,
								"Could not allocate memory for rubik\n");
						}
						return;
					}
					rp->shufflingmoves++;
					rp->movement.face = NO_FACE;
				}
			}
		}
	} else {
		if (rp->done) {
			if (++rp->rotatestep > DELAY_AFTER_SOLVING)
				if (!shuffle(mi)) {
					free_rubik(rp);
					if (MI_IS_VERBOSE(mi)) {
						(void) fprintf(stderr,
							"Could not allocate memory for rubik\n");
					}
					return;
				}
		} else {
			if (rp->movement.face == NO_FACE) {
				if (rp->storedmoves > 0) {
					rp->rotatestep = 0;
					rp->movement = rp->moves[rp->storedmoves - 1];
					rp->movement.direction = (rp->movement.direction +
						(MAXORIENT / 2)) % MAXORIENT;
				} else {
					rp->rotatestep = 0;
					rp->done = 1;
				}
			} else {
				if (rp->rotatestep == 0) {
					if (rp->movement.direction == CW || rp->movement.direction == CCW)
						rp->degreeTurn = (checkFaceSquare(rp, rp->movement.face)) ? 90 : 180;
					else
						rp->degreeTurn = (checkFaceSquare(rp, rowToRotate[rp->movement.face][rp->movement.direction])) ? 90 : 180;
				}
				rp->rotatestep += rp->anglestep;
				if (rp->rotatestep > rp->degreeTurn) {
					if (!evalmovement(mi, rp->movement)) {
						free_rubik(rp);
						if (MI_IS_VERBOSE(mi)) {
							(void) fprintf(stderr,
								"Could not allocate memory for rubik\n");
						}
						return;
					}
					rp->storedmoves--;
					rp->movement.face = NO_FACE;
				}
			}
		}
	}

	glPopMatrix();

	glFlush();

	rp->step += 0.05;
}

void
change_rubik(ModeInfo * mi)
{
	rubikstruct *rp;

	if (rubik == NULL)
		return;
	rp = &rubik[MI_SCREEN(mi)];

	if (!rp->glx_context)
		return;
	if (!pinit(mi)) {
		free_rubik(rp);
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr,
				"Could not allocate memory for rubik\n");
		}
		return;
	}
}

#endif
