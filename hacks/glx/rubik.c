/* -*- Mode: C; tab-width: 4 -*- */
/* rubik --- Shows a auto-solving Rubik's cube */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)rubik.c	4.07 97/11/24 xlockmore";

#endif

#undef DEBUG_LISTS
#undef LMN

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
 * This mode shows a auto-solving rubik's cube "puzzle". If somebody
 * intends to make a game or something based on this code, please let me
 * know first, my e-mail address is provided in this comment. Marcelo.
 *
 * Thanks goes also to Brian Paul for making it possible and inexpensive
 * to use OpenGL at home.
 *
 * Since I'm not a native English speaker, my apologies for any grammatical
 * mistake.
 *
 * My e-mail addresses are
 * vianna@cat.cbpf.br 
 *         and
 * m-vianna@usa.net
 *
 * Marcelo F. Vianna (Jul-31-1997)
 *
 * Revision History:
 * 08-Aug-97: Now has some internals from xrubik by David Bagley
 *            This should make it easier to add features.
 * 02-Aug-97: Now behaves more like puzzle.c: first show the cube being
 *            shuffled and then being solved. A mode specific option was added:
 *            "+/-hideshuffling" to provide the original behavior (in which
 *            only the solution is shown).
 *            The color labels corners are now rounded.
 *            Optimized the cubit() routine using glLists.
 * 01-Aug-97: Shuffling now avoids movements that undoes the previous movement
 *            and three consecutive identical moves (which is pretty stupid).
 *            improved the "cycles" option in replacement of David's hack,
 *            now rp->anglestep is a GLfloat, so this option selects the
 *            "exact" number of frames that a rotation (movement) takes to
 *            complete.
 * 30-Jul-97: Initial release, there is no algorithm to solve the puzzle,
 *            instead, it randomly shuffle the cube and then make the
 *            movements in the reverse order.
 *            The mode was written in 1 day (I got sick and had the day off).
 *            There was not much to do since I could not leave home... :)
 *
 */

/*-
 * Color labels mapping:
 * =====================
 *
 *                       +------------+
 *                       |0-->        |
 *                       ||           |
 *                       |v           |
 *                       |   TOP(0)   |
 *                       |            |
 *                       |            |
 *                       |           8|
 *           +-----------+------------+-----------+
 *           |0-->       |0-->        |0-->       |
 *           ||          ||           ||          |
 *           |v          |v           |v          |
 *           |  LEFT(1)  |  FRONT(2)  |  RIGHT(3) |
 *           |           |            |           |
 *           |           |            |           |
 *           |          8|           8|          8|
 *           +-----------+------------+-----------+
 *                       |0-->        |
 *                       ||           |
 *                       |v           |
 *                       |  BOTTOM(4) |  rp->faces[N][X+AVSIZE*Y]=
 *                       |            |         rp->cubeLoc[N][X+AVSIZE*Y]=
 *                       |            | 
 *                       |           8|         +---+---+---+
 *                       +------------+         |   |   |   |
 *                       |0-->        |         | 0 | 1 | 2 |
 *                       ||           |         |---+---+---+
 *                       |v           |         |  xxxxx(N) |
 *                       |   BACK(5)  |         | 3 | 4 | 5 |
 *                       |            |         +---+---+---+
 *                       |            |         |   |   |  |
 *                       |           8|         | 6 | 7 | 8 |
 *                       +------------+         +---+---+---+
 *
 *  Map to 3d
 *  FRONT  => X, Y
 *  BACK   => X, Y
 *  LEFT   => Z, Y
 *  RIGHT  => Z, Y
 *  TOP    => X, Z
 *  BOTTOM => X, Z
 */

/*-
 * PURIFY 3.0a on SunOS4 reports an unitialized memory read on each of
 * the glCallList() functions below when using MesaGL 2.1.  This has
 * been fixed in MesaGL 2.2 and later releases.
 */

/*-
 * due to a Bug/feature in VMS X11/Intrinsic.h has to be placed before xlock.
 * otherwise caddr_t is not defined correctly
 */
#include <X11/Intrinsic.h>

#ifdef STANDALONE
# define PROGCLASS	"Rubik"
# define HACK_INIT	init_rubik
# define HACK_DRAW	draw_rubik
# define rubik_opts	xlockmore_opts
# define DEFAULTS	"*delay: 40000 \n"		\
					"*count: -30 \n"		\
					"*cycles: 5 \n"			\
					"*size:  -6 \n"
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#define DEF_HIDESHUFFLING     "False"

static Bool hideshuffling;

static XrmOptionDescRec opts[] =
{
  {"-hideshuffling", ".rubik.hideshuffling", XrmoptionNoArg, (caddr_t) "on"},
  {"+hideshuffling", ".rubik.hideshuffling", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(caddr_t *) & hideshuffling, "hideshuffling", "Hideshuffling", DEF_HIDESHUFFLING, t_Bool}
};

static OptionStruct desc[] =
{
	{"-/+hideshuffling", "turn on/off hidden shuffle phase"}
};

ModeSpecOpt rubik_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   rubik_description =
{"rubik", "init_rubik", "draw_rubik", "release_rubik",
 "draw_rubik", "change_rubik", NULL, &rubik_opts,
 1000, -30, 5, -6, 4, 1.0, "",
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
#ifdef LMN			/* LxMxN not completed yet... */
#define MAXSIZEX (rp->sizex)
#define MAXSIZEY (rp->sizey)
#define MAXSIZEZ (rp->sizez)
#define AVSIZE (rp->avsize)
#define MAXSIZE (rp->maxsize)
#define AVSIZESQ (rp->avsizeSq)
#define MAXSIZESQ (rp->maxsizeSq)
#else
#define MAXSIZEX (rp->size)
#define MAXSIZEY (rp->size)
#define MAXSIZEZ (rp->size)
#define AVSIZE (rp->size)
#define MAXSIZE (rp->size)
#define AVSIZESQ (rp->sizeSq)
#define MAXSIZESQ (rp->sizeSq)
#endif
#define MAXSIZEXY (MAXSIZEX*MAXSIZEY)
#define MAXSIZEZY (MAXSIZEZ*MAXSIZEY)
#define MAXSIZEXZ (MAXSIZEX*MAXSIZEZ)
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
#define CCW (2*MAXORIENT-1)

#define TOP_FACE 0
#define LEFT_FACE 1
#define FRONT_FACE 2
#define RIGHT_FACE 3
#define BOTTOM_FACE 4
#define BACK_FACE 5
#define NO_FACE (MAXFACES)
#define NO_ROTATION (2*MAXORIENT)
#define NO_DEPTH MAXSIZE

#define REVX(a) (MAXSIZEX - a - 1)
#define REVY(a) (MAXSIZEY - a - 1)
#define REVZ(a) (MAXSIZEZ - a - 1)

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
 * Beware.. using this for NxNxN makes some assumptions that referenced
 * cubes are along the diagonal top-left to bottom-right.
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
	{0, BOTTOM, LEFT},
	{0, RIGHT, BOTTOM},
	{0, TOP, RIGHT},
	{1, RIGHT, BOTTOM},
	{0, LEFT, TOP}
};

typedef struct {
	GLint       WindH, WindW;
	GLfloat     step;
	RubikMove  *moves;
	int         storedmoves;
	int         shufflingmoves;
#ifdef LMN			/* Under construction */
	int         sizex, sizey, sizez;
	int         avsize, maxsize;
	int         avsizeSq, maxsizeSq;
#else
	int         size, sizeSq;
#endif
	int         action;
	int         done;
	GLfloat     anglestep;
	RubikLoc   *cubeLoc[MAXFACES];
	RubikLoc   *rowLoc[MAXORIENT];
	RubikMove   movement;
	GLfloat     rotatestep;
	GLXContext *glx_context;
	int         AreObjectsDefined[1];
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

static rubikstruct *rubik = NULL;
static GLuint objects;

#define ObjCubit        0

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
draw_cubit(ModeInfo * mi,
	   int back, int front, int left, int right, int bottom, int top)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];
	int         mono = MI_IS_MONO(mi);

	if (!rp->AreObjectsDefined[ObjCubit]) {
		glNewList(objects + ObjCubit, GL_COMPILE_AND_EXECUTE);
		glBegin(GL_QUADS);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
		glNormal3f(0.00, 0.00, 1.00);
		glVertex3f(-0.45, -0.45, 0.50);
		glVertex3f(0.45, -0.45, 0.50);
		glVertex3f(0.45, 0.45, 0.50);
		glVertex3f(-0.45, 0.45, 0.50);
		glNormal3f(0.00, 0.00, -1.00);
		glVertex3f(-0.45, 0.45, -0.50);
		glVertex3f(0.45, 0.45, -0.50);
		glVertex3f(0.45, -0.45, -0.50);
		glVertex3f(-0.45, -0.45, -0.50);
		glNormal3f(-1.00, 0.00, 0.00);
		glVertex3f(-0.50, -0.45, 0.45);
		glVertex3f(-0.50, 0.45, 0.45);
		glVertex3f(-0.50, 0.45, -0.45);
		glVertex3f(-0.50, -0.45, -0.45);
		glNormal3f(1.00, 0.00, 0.00);
		glVertex3f(0.50, -0.45, -0.45);
		glVertex3f(0.50, 0.45, -0.45);
		glVertex3f(0.50, 0.45, 0.45);
		glVertex3f(0.50, -0.45, 0.45);
		glNormal3f(0.00, -1.00, 0.00);
		glVertex3f(0.45, -0.50, -0.45);
		glVertex3f(0.45, -0.50, 0.45);
		glVertex3f(-0.45, -0.50, 0.45);
		glVertex3f(-0.45, -0.50, -0.45);
		glNormal3f(0.00, 1.00, 0.00);
		glVertex3f(-0.45, 0.50, -0.45);
		glVertex3f(-0.45, 0.50, 0.45);
		glVertex3f(0.45, 0.50, 0.45);
		glVertex3f(0.45, 0.50, -0.45);
		glNormal3f(-1.00, -1.00, 0.00);
		glVertex3f(-0.45, -0.50, -0.45);
		glVertex3f(-0.45, -0.50, 0.45);
		glVertex3f(-0.50, -0.45, 0.45);
		glVertex3f(-0.50, -0.45, -0.45);
		glNormal3f(1.00, 1.00, 0.00);
		glVertex3f(0.45, 0.50, -0.45);
		glVertex3f(0.45, 0.50, 0.45);
		glVertex3f(0.50, 0.45, 0.45);
		glVertex3f(0.50, 0.45, -0.45);
		glNormal3f(-1.00, 1.00, 0.00);
		glVertex3f(-0.50, 0.45, -0.45);
		glVertex3f(-0.50, 0.45, 0.45);
		glVertex3f(-0.45, 0.50, 0.45);
		glVertex3f(-0.45, 0.50, -0.45);
		glNormal3f(1.00, -1.00, 0.00);
		glVertex3f(0.50, -0.45, -0.45);
		glVertex3f(0.50, -0.45, 0.45);
		glVertex3f(0.45, -0.50, 0.45);
		glVertex3f(0.45, -0.50, -0.45);
		glNormal3f(0.00, -1.00, -1.00);
		glVertex3f(-0.45, -0.45, -0.50);
		glVertex3f(0.45, -0.45, -0.50);
		glVertex3f(0.45, -0.50, -0.45);
		glVertex3f(-0.45, -0.50, -0.45);
		glNormal3f(0.00, 1.00, 1.00);
		glVertex3f(-0.45, 0.45, 0.50);
		glVertex3f(0.45, 0.45, 0.50);
		glVertex3f(0.45, 0.50, 0.45);
		glVertex3f(-0.45, 0.50, 0.45);
		glNormal3f(0.00, -1.00, 1.00);
		glVertex3f(-0.45, -0.50, 0.45);
		glVertex3f(0.45, -0.50, 0.45);
		glVertex3f(0.45, -0.45, 0.50);
		glVertex3f(-0.45, -0.45, 0.50);
		glNormal3f(0.00, 1.00, -1.00);
		glVertex3f(-0.45, 0.50, -0.45);
		glVertex3f(0.45, 0.50, -0.45);
		glVertex3f(0.45, 0.45, -0.50);
		glVertex3f(-0.45, 0.45, -0.50);
		glNormal3f(-1.00, 0.00, -1.00);
		glVertex3f(-0.50, -0.45, -0.45);
		glVertex3f(-0.50, 0.45, -0.45);
		glVertex3f(-0.45, 0.45, -0.50);
		glVertex3f(-0.45, -0.45, -0.50);
		glNormal3f(1.00, 0.00, 1.00);
		glVertex3f(0.50, -0.45, 0.45);
		glVertex3f(0.50, 0.45, 0.45);
		glVertex3f(0.45, 0.45, 0.50);
		glVertex3f(0.45, -0.45, 0.50);
		glNormal3f(1.00, 0.00, -1.00);
		glVertex3f(0.45, -0.45, -0.50);
		glVertex3f(0.45, 0.45, -0.50);
		glVertex3f(0.50, 0.45, -0.45);
		glVertex3f(0.50, -0.45, -0.45);
		glNormal3f(-1.00, 0.00, 1.00);
		glVertex3f(-0.45, -0.45, 0.50);
		glVertex3f(-0.45, 0.45, 0.50);
		glVertex3f(-0.50, 0.45, 0.45);
		glVertex3f(-0.50, -0.45, 0.45);
		glEnd();
		glBegin(GL_TRIANGLES);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
		glNormal3f(1.00, 1.00, 1.00);
		glVertex3f(0.45, 0.45, 0.50);
		glVertex3f(0.50, 0.45, 0.45);
		glVertex3f(0.45, 0.50, 0.45);
		glNormal3f(-1.00, -1.00, -1.00);
		glVertex3f(-0.45, -0.50, -0.45);
		glVertex3f(-0.50, -0.45, -0.45);
		glVertex3f(-0.45, -0.45, -0.50);
		glNormal3f(-1.00, 1.00, 1.00);
		glVertex3f(-0.45, 0.45, 0.50);
		glVertex3f(-0.45, 0.50, 0.45);
		glVertex3f(-0.50, 0.45, 0.45);
		glNormal3f(1.00, -1.00, -1.00);
		glVertex3f(0.50, -0.45, -0.45);
		glVertex3f(0.45, -0.50, -0.45);
		glVertex3f(0.45, -0.45, -0.50);
		glNormal3f(1.00, -1.00, 1.00);
		glVertex3f(0.45, -0.45, 0.50);
		glVertex3f(0.45, -0.50, 0.45);
		glVertex3f(0.50, -0.45, 0.45);
		glNormal3f(-1.00, 1.00, -1.00);
		glVertex3f(-0.50, 0.45, -0.45);
		glVertex3f(-0.45, 0.50, -0.45);
		glVertex3f(-0.45, 0.45, -0.50);
		glNormal3f(-1.00, -1.00, 1.00);
		glVertex3f(-0.45, -0.45, 0.50);
		glVertex3f(-0.50, -0.45, 0.45);
		glVertex3f(-0.45, -0.50, 0.45);
		glNormal3f(1.00, 1.00, -1.00);
		glVertex3f(0.50, 0.45, -0.45);
		glVertex3f(0.45, 0.45, -0.50);
		glVertex3f(0.45, 0.50, -0.45);
		glEnd();
		glEndList();
		rp->AreObjectsDefined[ObjCubit] = 1;
#ifdef DEBUG_LISTS
		(void) printf("Cubit drawn SLOWLY\n");
#endif
	} else {
		glCallList(objects + ObjCubit);
#ifdef DEBUG_LISTS
		(void) printf("Cubit drawn quickly\n");
#endif
	}

	if (back != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(back, mono);
		glNormal3f(0.00, 0.00, -1.00);
		glVertex3f(-0.35, 0.40, -0.51);
		glVertex3f(0.35, 0.40, -0.51);
		glVertex3f(0.40, 0.35, -0.51);
		glVertex3f(0.40, -0.35, -0.51);
		glVertex3f(0.35, -0.40, -0.51);
		glVertex3f(-0.35, -0.40, -0.51);
		glVertex3f(-0.40, -0.35, -0.51);
		glVertex3f(-0.40, 0.35, -0.51);
		glEnd();
	}
	if (front != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(front, mono);
		glNormal3f(0.00, 0.00, 1.00);
		glVertex3f(-0.35, -0.40, 0.51);
		glVertex3f(0.35, -0.40, 0.51);
		glVertex3f(0.40, -0.35, 0.51);
		glVertex3f(0.40, 0.35, 0.51);
		glVertex3f(0.35, 0.40, 0.51);
		glVertex3f(-0.35, 0.40, 0.51);
		glVertex3f(-0.40, 0.35, 0.51);
		glVertex3f(-0.40, -0.35, 0.51);
		glEnd();
	}
	if (left != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(left, mono);
		glNormal3f(-1.00, 0.00, 0.00);
		glVertex3f(-0.51, -0.35, 0.40);
		glVertex3f(-0.51, 0.35, 0.40);
		glVertex3f(-0.51, 0.40, 0.35);
		glVertex3f(-0.51, 0.40, -0.35);
		glVertex3f(-0.51, 0.35, -0.40);
		glVertex3f(-0.51, -0.35, -0.40);
		glVertex3f(-0.51, -0.40, -0.35);
		glVertex3f(-0.51, -0.40, 0.35);
		glEnd();
	}
	if (right != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(right, mono);
		glNormal3f(1.00, 0.00, 0.00);
		glVertex3f(0.51, -0.35, -0.40);
		glVertex3f(0.51, 0.35, -0.40);
		glVertex3f(0.51, 0.40, -0.35);
		glVertex3f(0.51, 0.40, 0.35);
		glVertex3f(0.51, 0.35, 0.40);
		glVertex3f(0.51, -0.35, 0.40);
		glVertex3f(0.51, -0.40, 0.35);
		glVertex3f(0.51, -0.40, -0.35);
		glEnd();
	}
	if (bottom != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(bottom, mono);
		glNormal3f(0.00, -1.00, 0.00);
		glVertex3f(0.40, -0.51, -0.35);
		glVertex3f(0.40, -0.51, 0.35);
		glVertex3f(0.35, -0.51, 0.40);
		glVertex3f(-0.35, -0.51, 0.40);
		glVertex3f(-0.40, -0.51, 0.35);
		glVertex3f(-0.40, -0.51, -0.35);
		glVertex3f(-0.35, -0.51, -0.40);
		glVertex3f(0.35, -0.51, -0.40);
		glEnd();
	}
	if (top != NO_FACE) {
		glBegin(GL_POLYGON);
		pickcolor(top, mono);
		glNormal3f(0.00, 1.00, 0.00);
		glVertex3f(-0.40, 0.51, -0.35);
		glVertex3f(-0.40, 0.51, 0.35);
		glVertex3f(-0.35, 0.51, 0.40);
		glVertex3f(0.35, 0.51, 0.40);
		glVertex3f(0.40, 0.51, 0.35);
		glVertex3f(0.40, 0.51, -0.35);
		glVertex3f(0.35, 0.51, -0.40);
		glVertex3f(-0.35, 0.51, -0.40);
		glEnd();
	}
}


static      RubikSlice
convertMove(rubikstruct * rp, RubikMove move)
{
	RubikSlice  slice;
	RubikLoc    plane;

	plane = rotateSlice[(int) move.face][move.direction % 2];
	slice.face = plane.face;
	slice.rotation = plane.rotation;
	if (slice.rotation == CW)	/* I just know this to be true... */
		slice.depth = AVSIZESQ - 1 - move.position;
	else
		slice.depth = move.position;
	slice.depth = slice.depth / AVSIZE;
	/* If slice.depth = 0 then face 0, face 1, or face 2 moves */
	if (move.direction / 2)
		slice.rotation = (plane.rotation == CW) ? CCW : CW;
	return slice;
}

/* Assume for the moment that the size is at least 2 */
static void
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
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];
	RubikSlice  slice;
	GLfloat     rotatestep;
	int         i, j, k;

	if (rp->movement.face == NO_FACE) {
		slice.face = NO_FACE;
		slice.rotation = NO_ROTATION;
		slice.depth = NO_DEPTH;
	} else {
		slice = convertMove(rp, rp->movement);
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
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, 0, S1);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * LASTY].face, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
			}
			glTranslatef(0, 0, S1);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * LASTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, -SZ);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * FIRSTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * LASTZ].face, NO_FACE);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, 0, S1);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * REVZ(k)].face, NO_FACE);
				}
				glTranslatef(0, 0, S1);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * LASTY].face,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * FIRSTZ].face, NO_FACE);
			}
			glTranslatef(1, 0, -SZ);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, 0, S1);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * LASTY].face,
					   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
			}
			glTranslatef(0, 0, S1);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * LASTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			glPopMatrix();
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glPushMatrix();
				if (slice.depth == REVY(j))
					glRotatef(rotatestep, 0, HALFY, 0);
				glTranslatef(-HALFX, MIDY(j), -HALFZ);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * j].face, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, 0, S1);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * REVY(j)].face, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, 0, S1);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * REVY(j)].face,
					   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(1, 0, -SZ);
					draw_cubit(mi,
						   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * j].face, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
					/* Center */
					glTranslatef(0, 0, SZ);
					draw_cubit(mi,
						   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * REVY(j)].face,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(S1, 0, -SZ);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * j].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, 0, S1);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * REVY(j)].face,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, 0, S1);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * REVY(j)].face,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
				glPopMatrix();
			}
			if (slice.depth == 0)
				glRotatef(rotatestep, 0, HALFY, 0);

			glTranslatef(-HALFX, HALFY, -HALFZ);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * LASTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, 0, S1);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * FIRSTY].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * k].face);
			}
			glTranslatef(0, 0, S1);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * FIRSTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * LASTZ].face);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, -SZ);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * LASTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * FIRSTZ].face);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, 0, S1);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * k].face);
				}
				glTranslatef(0, 0, S1);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * FIRSTY].face,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * LASTZ].face);
			}
			glTranslatef(S1, 0, -SZ);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * LASTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * FIRSTZ].face);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, 0, S1);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * FIRSTY].face,
					   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * k].face);
			}
			glTranslatef(0, 0, S1);
			draw_cubit(mi,
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
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(0, S1, 0);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * j].face, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(0, S1, 0);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * LASTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, -SY, S1);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * LASTY].face, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(0, S1, 0);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * REVY(j)].face, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, S1, 0);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * FIRSTY].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * k].face);
			}
			glTranslatef(0, -SY, S1);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * LASTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(0, S1, 0);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * REVY(j)].face,
					   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(0, S1, 0);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * FIRSTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * LASTZ].face);
			glPopMatrix();
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glPushMatrix();
				if (slice.depth == i)
					glRotatef(-rotatestep, HALFX, 0, 0);
				glTranslatef(MIDX(i), -HALFY, -HALFZ);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * FIRSTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * LASTZ].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(0, S1, 0);
					draw_cubit(mi,
						   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * j].face, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, S1, 0);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * LASTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * FIRSTZ].face);
				for (k = 1; k < MAXSIZEZ - 1; k++) {
					glTranslatef(0, -SY, S1);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * REVZ(k)].face, NO_FACE);
					/* Center */
					glTranslatef(0, SY, 0);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * k].face);
				}
				glTranslatef(0, -SY, S1);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * LASTY].face,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * FIRSTZ].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(0, S1, 0);
					draw_cubit(mi,
						   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * REVY(j)].face,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, S1, 0);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * FIRSTY].face,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * LASTZ].face);
				glPopMatrix();
			}
			if (slice.depth == MAXSIZEX - 1)
				glRotatef(-rotatestep, HALFX, 0, 0);
			glTranslatef(HALFX, -HALFY, -HALFZ);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(0, S1, 0);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * j].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(0, S1, 0);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * LASTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * FIRSTZ].face);
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glTranslatef(0, -SY, S1);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * LASTY].face,
					   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(0, S1, 0);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * REVY(j)].face,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(0, S1, 0);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * FIRSTY].face,
					   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * k].face);
			}
			glTranslatef(0, -SY, S1);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * LASTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(0, S1, 0);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * REVY(j)].face,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(0, S1, 0);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * LASTZ].face);
			break;
		case FRONT_FACE:	/* BACK_FACE too */
			glPushMatrix();
			if (slice.depth == MAXSIZEZ - 1)
				glRotatef(rotatestep, 0, 0, HALFZ);

			glTranslatef(-HALFX, -HALFY, -HALFZ);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, 0);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * FIRSTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * LASTZ].face, NO_FACE);
			}
			glTranslatef(S1, 0, 0);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * LASTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(-SX, S1, 0);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * j].face, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(S1, 0, 0);
					draw_cubit(mi,
						   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * j].face, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(S1, 0, 0);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * j].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(-SX, S1, 0);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][FIRSTX + MAXSIZEX * LASTY].face, NO_FACE,
				   rp->cubeLoc[LEFT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, 0);
				draw_cubit(mi,
					   rp->cubeLoc[BACK_FACE][i + MAXSIZEX * LASTY].face, NO_FACE,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * FIRSTZ].face);
			}
			glTranslatef(S1, 0, 0);
			draw_cubit(mi,
				   rp->cubeLoc[BACK_FACE][LASTX + MAXSIZEX * LASTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * FIRSTZ].face);
			glPopMatrix();
			for (k = 1; k < MAXSIZEZ - 1; k++) {
				glPushMatrix();
				if (slice.depth == REVZ(k))
					glRotatef(rotatestep, 0, 0, HALFZ);
				glTranslatef(-HALFX, -HALFY, MIDZ(k));
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * LASTY].face, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(S1, 0, 0);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * REVZ(k)].face, NO_FACE);
				}
				glTranslatef(S1, 0, 0);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * LASTY].face,
					   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * REVZ(k)].face, NO_FACE);
				for (j = 1; j < MAXSIZEY - 1; j++) {
					glTranslatef(-SX, S1, 0);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * REVY(j)].face, NO_FACE,
						   NO_FACE, NO_FACE);
					/* Center */
					glTranslatef(SX, 0, 0);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * REVY(j)].face,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(-SX, S1, 0);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[LEFT_FACE][k + MAXSIZEZ * FIRSTY].face, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * k].face);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(S1, 0, 0);
					draw_cubit(mi,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE,
						   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * k].face);
				}
				glTranslatef(S1, 0, 0);
				draw_cubit(mi,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][REVZ(k) + MAXSIZEZ * FIRSTY].face,
					   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * k].face);
				glPopMatrix();
			}
			if (slice.depth == 0)
				glRotatef(rotatestep, 0, 0, HALFZ);
			glTranslatef(-HALFX, -HALFY, HALFZ);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * LASTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * LASTY].face, NO_FACE,
				   rp->cubeLoc[BOTTOM_FACE][FIRSTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, 0);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * LASTY].face,
					   NO_FACE, NO_FACE,
					   rp->cubeLoc[BOTTOM_FACE][i + MAXSIZEX * FIRSTZ].face, NO_FACE);
			}
			glTranslatef(S1, 0, 0);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * LASTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * LASTY].face,
				   rp->cubeLoc[BOTTOM_FACE][LASTX + MAXSIZEX * FIRSTZ].face, NO_FACE);
			for (j = 1; j < MAXSIZEY - 1; j++) {
				glTranslatef(-SX, S1, 0);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * REVY(j)].face,
					   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * REVY(j)].face, NO_FACE,
					   NO_FACE, NO_FACE);
				for (i = 1; i < MAXSIZEX - 1; i++) {
					glTranslatef(S1, 0, 0);
					draw_cubit(mi,
						   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * REVY(j)].face,
						   NO_FACE, NO_FACE,
						   NO_FACE, NO_FACE);
				}
				glTranslatef(S1, 0, 0);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * REVY(j)].face,
					   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * REVY(j)].face,
					   NO_FACE, NO_FACE);
			}
			glTranslatef(-SX, S1, 0);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][FIRSTX + MAXSIZEX * FIRSTY].face,
				   rp->cubeLoc[LEFT_FACE][LASTZ + MAXSIZEZ * FIRSTY].face, NO_FACE,
				   NO_FACE, rp->cubeLoc[TOP_FACE][FIRSTX + MAXSIZEX * LASTZ].face);
			for (i = 1; i < MAXSIZEX - 1; i++) {
				glTranslatef(S1, 0, 0);
				draw_cubit(mi,
					   NO_FACE, rp->cubeLoc[FRONT_FACE][i + MAXSIZEX * FIRSTY].face,
					   NO_FACE, NO_FACE,
					   NO_FACE, rp->cubeLoc[TOP_FACE][i + MAXSIZEX * LASTZ].face);
			}
			glTranslatef(S1, 0, 0);
			draw_cubit(mi,
				   NO_FACE, rp->cubeLoc[FRONT_FACE][LASTX + MAXSIZEX * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[RIGHT_FACE][FIRSTZ + MAXSIZEZ * FIRSTY].face,
				   NO_FACE, rp->cubeLoc[TOP_FACE][LASTX + MAXSIZEX * LASTZ].face);
			break;
	}
#undef S1
}

/* From David Bagley's xrubik.  Used by permission. ;)  */
static void
readRC(rubikstruct * rp, int face, int dir, int h, int orient, int size)
{
	int         g;

	if (dir == TOP || dir == BOTTOM)
		for (g = 0; g < size; g++)
			rp->rowLoc[orient][g] =
				rp->cubeLoc[face][g * size + h];
	else			/* dir == RIGHT || dir == LEFT */
		for (g = 0; g < size; g++)
			rp->rowLoc[orient][g] =
				rp->cubeLoc[face][h * size + g];
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
	int         g, position;

	if (dir == TOP || dir == BOTTOM) {
		for (g = 0; g < size; g++) {
			position = g * size + h;
			rp->cubeLoc[face][position] = rp->rowLoc[orient][g];
			/* DrawSquare(face, position); */
		}
	} else {		/* dir == RIGHT || dir == LEFT */
		for (g = 0; g < size; g++) {
			position = h * size + g;
			rp->cubeLoc[face][position] = rp->rowLoc[orient][g];
			/* DrawSquare(face, position); */
		}
	}
}

static void
rotateFace(rubikstruct * rp, int face, int direction)
{
	int         position, i, j;
	RubikLoc   *faceLoc = NULL;

	if ((faceLoc = (RubikLoc *) malloc(AVSIZESQ * sizeof (RubikLoc))) == NULL)
		(void) fprintf(stderr,
		 "Could not allocate memory for rubik face position info\n");
	/* Read Face */
	for (position = 0; position < AVSIZESQ; position++)
		faceLoc[position] = rp->cubeLoc[face][position];
	/* Write Face */
	for (position = 0; position < AVSIZESQ; position++) {
		i = position % AVSIZE;
		j = position / AVSIZE;
		rp->cubeLoc[face][position] = (direction == CW) ?
			faceLoc[(AVSIZE - i - 1) * AVSIZE + j] :
			faceLoc[i * AVSIZE + AVSIZE - j - 1];
		rp->cubeLoc[face][position].rotation =
			(rp->cubeLoc[face][position].rotation + direction - MAXORIENT) %
			MAXORIENT;
		/* DrawSquare(face, position); */
	}
	if (faceLoc != NULL)
		(void) free((void *) faceLoc);
}

static void
moveRubik(rubikstruct * rp, int face, int direction, int position)
{
	int         newFace, newDirection, rotate, reverse = False;
	int         h, k, newH = 0;
	int         i, j;

	if (direction == CW || direction == CCW) {
		direction = (direction == CCW) ?
			(rotateToRow[face].direction + 2) % MAXORIENT :
			rotateToRow[face].direction;
		i = j = (rotateToRow[face].sideFace == RIGHT ||
		      rotateToRow[face].sideFace == BOTTOM) ? AVSIZE - 1 : 0;
		face = rotateToRow[face].face;
		position = j * AVSIZE + i;
	}
	i = position % AVSIZE;
	j = position / AVSIZE;
	h = (direction == TOP || direction == BOTTOM) ? i : j;
	/* rotate sides CW or CCW */
	if (h == AVSIZE - 1) {
		newDirection = (direction == TOP || direction == BOTTOM) ?
			TOP : RIGHT;
		if (direction == TOP || direction == RIGHT)
			rotateFace(rp, rowToRotate[face][newDirection], CW);
		else		/* direction == BOTTOM || direction == LEFT */
			rotateFace(rp, rowToRotate[face][newDirection], CCW);
	}
	if (h == 0) {
		newDirection = (direction == TOP || direction == BOTTOM) ?
			BOTTOM : LEFT;
		if (direction == TOP || direction == RIGHT)
			rotateFace(rp, rowToRotate[face][newDirection], CCW);
		else		/* direction == BOTTOM  || direction == LEFT */
			rotateFace(rp, rowToRotate[face][newDirection], CW);
	}
	/* Slide rows */
	readRC(rp, face, direction, h, 0, AVSIZE);
	for (k = 1; k <= MAXORIENT; k++) {
		newFace = slideNextRow[face][direction].face;
		rotate = slideNextRow[face][direction].rotation;
		newDirection = (rotate + direction) % MAXORIENT;
		switch (rotate) {
			case TOP:
				newH = h;
				reverse = False;
				break;
			case RIGHT:
				if (newDirection == TOP || newDirection == BOTTOM) {
					newH = AVSIZE - 1 - h;
					reverse = False;
				} else {	/* newDirection == RIGHT || newDirection == LEFT */
					newH = h;
					reverse = True;
				}
				break;
			case BOTTOM:
				newH = AVSIZE - 1 - h;
				reverse = True;
				break;
			case LEFT:
				if (newDirection == TOP || newDirection == BOTTOM) {
					newH = h;
					reverse = True;
				} else {	/* newDirection == RIGHT || newDirection == LEFT */
					newH = AVSIZE - 1 - h;
					reverse = False;
				}
				break;
			default:
				(void) printf("moveRubik: rotate %d\n", rotate);
		}
		if (k != MAXORIENT)
			readRC(rp, newFace, newDirection, newH, k, AVSIZE);
		rotateRC(rp, rotate, k - 1, AVSIZE);
		if (reverse == True)
			reverseRC(rp, k - 1, AVSIZE);
		writeRC(rp, newFace, newDirection, newH, k - 1, AVSIZE);
		face = newFace;
		direction = newDirection;
		h = newH;
	}
}

#ifdef DEBUG
void
printCube(rubikstruct * rp)
{
	int         face, position;

	for (face = 0; face < MAXFACES; face++) {
		for (position = 0; position < AVSIZESQ; position++) {
			(void) printf("%d %d  ", rp->cubeLoc[face][position].face,
				      rp->cubeLoc[face][position].rotation);
			if (!((position + 1) % AVSIZE))
				(void) printf("\n");
		}
		(void) printf("\n");
	}
	(void) printf("\n");
}

#endif

static void
evalmovement(ModeInfo * mi, RubikMove movement)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];

#ifdef DEBUG
	printCube(rp);
#endif
	if (movement.face < 0 || movement.face >= MAXFACES)
		return;

	moveRubik(rp, movement.face, movement.direction, movement.position);

}

static      Bool
compare_moves(rubikstruct * rp, RubikMove move1, RubikMove move2, Bool opp)
{
	RubikSlice  slice1, slice2;

	slice1 = convertMove(rp, move1);
	slice2 = convertMove(rp, move2);
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

static void
shuffle(ModeInfo * mi)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];
	int         i, face, position;
	RubikMove   move;

	AVSIZE = MI_SIZE(mi);
	if (AVSIZE < -MINSIZE)
		AVSIZE = NRAND(-AVSIZE - MINSIZE + 1) + MINSIZE;
	else if (AVSIZE < MINSIZE)
		AVSIZE = MINSIZE;
	/* Let me waste a little space for the moment */
	/* Future cube to be LxMxN and not just NxNxN, but not done yet */
	AVSIZESQ = AVSIZE * AVSIZE;
#ifdef LMN
	MAXSIZEX = AVSIZE;
	MAXSIZEY = AVSIZE;
	MAXSIZEZ = AVSIZE;
	MAXSIZE = AVSIZE;
	MAXSIZESQ = AVSIZESQ;
#endif

	for (face = 0; face < MAXFACES; face++) {
		if (rp->cubeLoc[face] != NULL)
			(void) free((void *) rp->cubeLoc[face]);
		if ((rp->cubeLoc[face] =
		  (RubikLoc *) malloc(AVSIZESQ * sizeof (RubikLoc))) == NULL)
			(void) fprintf(stderr,
				       "Could not allocate memory for rubik cube position info\n");
		for (position = 0; position < AVSIZESQ; position++) {
			rp->cubeLoc[face][position].face = face;
			rp->cubeLoc[face][position].rotation = TOP;
		}
	}
	for (i = 0; i < MAXORIENT; i++) {
		if (rp->rowLoc[i] != NULL)
			(void) free((void *) rp->rowLoc[i]);
		if ((rp->rowLoc[i] =
		     (RubikLoc *) malloc(AVSIZE * sizeof (RubikLoc))) == NULL)
			(void) fprintf(stderr,
				       "Could not allocate memory for rubik row position info\n");
	}
	rp->storedmoves = MI_COUNT(mi);
	if (rp->storedmoves < 0) {
		if (rp->moves != NULL)
			(void) free((void *) rp->moves);
		rp->moves = NULL;
		rp->storedmoves = NRAND(-rp->storedmoves) + 1;
	}
	if ((rp->storedmoves) && (rp->moves == NULL))
		if ((rp->moves =
		     (RubikMove *) calloc(rp->storedmoves + 1, sizeof (RubikMove))) == NULL)
			(void) fprintf(stderr,
			"Could not allocate memory for rubik move buffer\n");

	if (MI_CYCLES(mi) <= 1) {
		rp->anglestep = 90.0;
	} else {
		rp->anglestep = 90.0 / (GLfloat) (MI_CYCLES(mi));
	}

	for (i = 0; i < rp->storedmoves; i++) {
		int         condition;

		do {
			move.face = NRAND(6);
			move.direction = NRAND(4);	/* Exclude CW and CCW, its ok */
			/*
			 * Randomize position along diagonal, each plane gets an equal chance.
			 * This trick will only work for NxNxN cubes
			 * draw_cube DEPENDS on that they are chosen this way.
			 */
			move.position = NRAND(AVSIZE) * (AVSIZE + 1);


			condition = 1;

			if (i > 0)	/* avoid immediate undoing moves */
				if (compare_moves(rp, move, rp->moves[i - 1], True))
					condition = 0;
			if (i > 1)	/* avoid 3 consecutive identical moves */
				if (compare_moves(rp, move, rp->moves[i - 1], False) &&
				    compare_moves(rp, move, rp->moves[i - 2], False))
					condition = 0;
			/*
			   * Still some silly moves being made....
			 */
		} while (!condition);
		if (hideshuffling)
			evalmovement(mi, move);
		rp->moves[i] = move;
	}
	rp->movement.face = NO_FACE;
	rp->rotatestep = 0;
	rp->action = hideshuffling ? ACTION_SOLVE : ACTION_SHUFFLE;
	rp->shufflingmoves = 0;
	rp->done = 0;
}

static void
reshape(ModeInfo * mi, int width, int height)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];

	glViewport(0, 0, rp->WindW = (GLint) width, rp->WindH = (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 15.0);
	glMatrixMode(GL_MODELVIEW);

	rp->AreObjectsDefined[ObjCubit] = 0;
}

static void
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

	shuffle(mi);
}

void
init_rubik(ModeInfo * mi)
{
	int         screen = MI_SCREEN(mi);
	rubikstruct *rp;

	if (rubik == NULL) {
		if ((rubik = (rubikstruct *) calloc(MI_NUM_SCREENS(mi),
					      sizeof (rubikstruct))) == NULL)
			return;
	}
	rp = &rubik[screen];
	rp->step = NRAND(90);

	if ((rp->glx_context = init_GL(mi)) != NULL) {

		reshape(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		objects = glGenLists(1);
		pinit(mi);
	} else {
		MI_CLEARWINDOW(mi);
	}
}

void
draw_rubik(ModeInfo * mi)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	if (!rp->glx_context)
		return;

	glDrawBuffer(GL_BACK);
	glXMakeCurrent(display, window, *(rp->glx_context));

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	glTranslatef(0.0, 0.0, -10.0);

	if (!MI_IS_ICONIC(mi)) {
		glScalef(Scale4Window * rp->WindH / rp->WindW, Scale4Window, Scale4Window);
	} else {
		glScalef(Scale4Iconic * rp->WindH / rp->WindW, Scale4Iconic, Scale4Iconic);
	}

	glRotatef(rp->step * 100, 1, 0, 0);
	glRotatef(rp->step * 95, 0, 1, 0);
	glRotatef(rp->step * 90, 0, 0, 1);

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
				rp->rotatestep += rp->anglestep;
				if (rp->rotatestep > 90) {
					evalmovement(mi, rp->movement);
					rp->shufflingmoves++;
					rp->movement.face = NO_FACE;
				}
			}
		}
	} else {
		if (rp->done) {
			if (++rp->rotatestep > DELAY_AFTER_SOLVING)
				shuffle(mi);
		} else {
			if (rp->movement.face == NO_FACE) {
				if (rp->storedmoves > 0) {
					rp->rotatestep = 0;
					rp->movement = rp->moves[rp->storedmoves - 1];
					rp->movement.direction = (rp->movement.direction + (MAXORIENT / 2)) %
						MAXORIENT;
				} else {
					rp->rotatestep = 0;
					rp->done = 1;
				}
			} else {
				rp->rotatestep += rp->anglestep;
				if (rp->rotatestep > 90) {
					evalmovement(mi, rp->movement);
					rp->storedmoves--;
					rp->movement.face = NO_FACE;
				}
			}
		}
	}

	draw_cube(mi);

	glPopMatrix();

	glFlush();

	glXSwapBuffers(display, window);

	rp->step += 0.05;
}

void
change_rubik(ModeInfo * mi)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];

	if (!rp->glx_context)
		return;
	pinit(mi);
}

void
release_rubik(ModeInfo * mi)
{
	if (rubik != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			rubikstruct *rp = &rubik[screen];
			int         i;

			for (i = 0; i < MAXFACES; i++)
				if (rp->cubeLoc[i] != NULL)
					(void) free((void *) rp->cubeLoc[i]);
			for (i = 0; i < MAXORIENT; i++)
				if (rp->rowLoc[i] != NULL)
					(void) free((void *) rp->rowLoc[i]);
			if (rp->moves != NULL)
				(void) free((void *) rp->moves);
		}
		(void) free((void *) rubik);
		rubik = NULL;
	}
	FreeAllGL(mi);
}

#endif
