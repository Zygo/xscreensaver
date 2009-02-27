/* -*- Mode: C; tab-width: 4 -*- */
/* rubik --- Shows a self-solving Rubik's cube */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)rubik.c	4.04 97/07/28 xlockmore";

#endif

#undef DEBUG_LISTS

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
 * This mode shows a self solving rubik's cube "puzzle". If somebody
 * intends to make a game or something based on this code, please let me
 * know first, my e-mail address is provided in this comment. Marcelo.
 *
 * Thanks goes also to Brian Paul for making it possible and inexpensive
 * to use OpenGL at home.
 *
 * Since I'm not a native english speaker, my apologies for any gramatical
 * mistake.
 *
 * My e-mail addresses are
 * vianna@cat.cbpf.br 
 *         and
 * marcelo@venus.rdc.puc-rio.br
 *
 * Marcelo F. Vianna (Jul-31-1997)
 *
 * Revision History:
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
 *            The mode was written in 1 day (I got sick and had a license
 *            at work...) There was not much to do since I could not exit
 *            from home... :)
 *
 */

/*-
 * Color labels mapping:
 * =====================
 *
 *                       +------------+
 *                       |          22|
 *                       |            |
 *                       |            |
 *                       |   TOP(0)   |
 *                       |^           |
 *                       ||           |
 *                       |00-->       |
 *           +-----------+------------+-----------+
 *           |         22|          22|         22|
 *           |           |            |           |
 *           |           |            |           |
 *           |  LEFT(1)  |  FRONT(2)  |  RIGHT(3) |
 *           |^          |^           |^          |
 *           ||          ||           ||          |
 *           |00-->      |00-->       |00-->      |
 *           +-----------+------------+-----------+
 *                       |          22|
 *                       |            |
 *                       |            |
 *                       |  BOTTOM(4) |  rp->faces[N][X][Y]=
 *                       |^           |         F_[N][X][Y]=
 *                       ||           | 
 *                       |00-->       |         +---+---+---+
 *                       +------------+         |   |   |XY |
 *                       |          22|         |02 |12 |22 |
 *                       |            |         |---+---+---+
 *                       |            |         |  xxxxx(N) |
 *                       |   BACK(5)  |         |01 |11 |21 |
 *                       |^           |         +---+---+---+
 *                       ||           |         |XY |   |   |
 *                       |00-->       |         |00 |10 |20 |
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
#include <string.h>

#ifdef STANDALONE
# define PROGCLASS	"Rubik"
# define HACK_INIT	init_rubik
# define HACK_DRAW	draw_rubik
# define rubik_opts	xlockmore_opts
# define DEFAULTS	"*delay: 50000 \n"		\
					"*count: -30 \n"		\
					"*cycles: 5 \n"
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
{2, opts, 1, vars, desc};

#define Scale4Window               0.3
#define Scale4Iconic               0.7

#define VectMul(X1,Y1,Z1,X2,Y2,Z2) (Y1)*(Z2)-(Z1)*(Y2),(Z1)*(X2)-(X1)*(Z2),(X1)*(Y2)-(Y1)*(X2)
#define sqr(A)                     ((A)*(A))

#ifndef Pi
#define Pi                         M_PI
#endif

#define NO_ROTATION    -1
#define TOP_ROTATION    0
#define LEFT_ROTATION   1
#define FRONT_ROTATION  2
#define RIGHT_ROTATION  3
#define BOTTOM_ROTATION 4
#define BACK_ROTATION   5

#define CLOCK_WISE      0
#define C_CLOCK_WISE    1

#define ACTION_SOLVE    1
#define ACTION_SHUFFLE  0

#define DELAY_AFTER_SHUFFLING  5
#define DELAY_AFTER_SOLVING   20

#define F_ rp->faces

/*************************************************************************/

/*-
 * Ignore trivial case, since it adds needless complications.
 * MAXSIZE must be 2 or greater.
 */

#define MAXSIZEX 3
#define MAXSIZEY 3
#define MAXSIZEZ 3
#define MAXSIZE (MAX(MAX(MAXSIZEX,MAXSIZEY),MAXSIZEZ))
#define MAXSIZEXY (MAXSIZEX*MAXSIZEY)
#define MAXSIZEZY (MAXSIZEZ*MAXSIZEY)
#define MAXSIZEXZ (MAXSIZEX*MAXSIZEZ)
#define MAXSIZESQ (MAX(MAX(MAXSIZEXY,MAXSIZEZY),MAXSIZEXZ))
#define LAST (MAXSIZE-1)
#define LASTX (MAXSIZEX-1)
#define LASTY (MAXSIZEY-1)
#define LASTZ (MAXSIZEZ-1)

typedef struct {
	GLint       WindH, WindW;
	GLfloat     step;
	char       *movedfaces;
	char       *movedorient;
	int         storedmoves;
	int         shufflingmoves;
	int         action;
	int         done;
	GLfloat     anglestep;
	char        faces[6][MAXSIZE][MAXSIZE];
	int         movement;
	int         orientation;
	GLfloat     rotatestep;
	GLXContext  glx_context;
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
{1.0, 0.5, 0.4, 1.0};

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

static rubikstruct *rubik = NULL;
static GLuint objects;

#define ObjCubit        0

static void
pickcolor(char C)
{
	switch (C) {
		case 'R':
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialRed);
			break;
		case 'G':
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGreen);
			break;
		case 'B':
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialBlue);
			break;
		case 'Y':
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialYellow);
			break;
#if 0
		case 'C':
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialCyan);
			break;
		case 'M':
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialMagenta);
			break;
#else
		case 'O':
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialOrange);
			break;
		case 'W':
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
			break;
#endif
	}
}

static void
draw_cubit(ModeInfo * mi, char BACK, char FRONT, char LEFT, char RIGHT, char BOTTOM, char TOP)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];

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

	if (BACK != ' ') {
		glBegin(GL_POLYGON);
		pickcolor(BACK);
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
	if (FRONT != ' ') {
		glBegin(GL_POLYGON);
		pickcolor(FRONT);
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
	if (LEFT != ' ') {
		glBegin(GL_POLYGON);
		pickcolor(LEFT);
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
	if (RIGHT != ' ') {
		glBegin(GL_POLYGON);
		pickcolor(RIGHT);
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
	if (BOTTOM != ' ') {
		glBegin(GL_POLYGON);
		pickcolor(BOTTOM);
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
	if (TOP != ' ') {
		glBegin(GL_POLYGON);
		pickcolor(TOP);
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
	glEnd();
}

static void
draw_cube(ModeInfo * mi)
{
#define S1 1
#define S2 (S1*2)
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];

	switch (rp->movement) {
		case NO_ROTATION:
		case BACK_ROTATION:
		case FRONT_ROTATION:
			glPushMatrix();
			if (rp->movement == BACK_ROTATION)
				glRotatef(-rp->rotatestep, 0, 0, 1);
			glTranslatef(-S1, -S1, -S1);
			draw_cubit(mi, F_[BACK_ROTATION][0][LAST], ' ',
				   F_[LEFT_ROTATION][0][0], ' ',
				   F_[BOTTOM_ROTATION][0][0], ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, F_[BACK_ROTATION][1][LAST], ' ',
				   ' ', ' ',
				   F_[BOTTOM_ROTATION][1][0], ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, F_[BACK_ROTATION][LAST][LAST], ' ',
				   ' ', F_[RIGHT_ROTATION][LAST][0],
				   F_[BOTTOM_ROTATION][LAST][0], ' ');
			glTranslatef(-S2, S1, 0);
			draw_cubit(mi, F_[BACK_ROTATION][0][1], ' ',
				   F_[LEFT_ROTATION][0][1], ' ',
				   ' ', ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, F_[BACK_ROTATION][1][1], ' ',
				   ' ', ' ',
				   ' ', ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, F_[BACK_ROTATION][LAST][1], ' ',
				   ' ', F_[RIGHT_ROTATION][LAST][1],
				   ' ', ' ');
			glTranslatef(-S2, S1, 0);
			draw_cubit(mi, F_[BACK_ROTATION][0][0], ' ',
				   F_[LEFT_ROTATION][0][LAST], ' ',
				   ' ', F_[TOP_ROTATION][0][LAST]);
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, F_[BACK_ROTATION][1][0], ' ',
				   ' ', ' ',
				   ' ', F_[TOP_ROTATION][1][LAST]);
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, F_[BACK_ROTATION][LAST][0], ' ',
				   ' ', F_[RIGHT_ROTATION][LAST][LAST],
				   ' ', F_[TOP_ROTATION][LAST][LAST]);
			glPopMatrix();
			glPushMatrix();
			glTranslatef(-S1, -S1, 0);
			draw_cubit(mi, ' ', ' ',
				   F_[LEFT_ROTATION][1][0], ' ',
				   F_[BOTTOM_ROTATION][0][1], ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', ' ',
				   ' ', ' ',
				   F_[BOTTOM_ROTATION][1][1], ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', ' ',
				   ' ', F_[RIGHT_ROTATION][1][0],
				   F_[BOTTOM_ROTATION][LAST][1], ' ');
			glTranslatef(-S2, S1, 0);
			draw_cubit(mi, ' ', ' ',
				   F_[LEFT_ROTATION][1][1], ' ',
				   ' ', ' ');
			glTranslatef(2, 0, 0);
			draw_cubit(mi, ' ', ' ',
				   ' ', F_[RIGHT_ROTATION][1][1],
				   ' ', ' ');
			glTranslatef(-S2, S1, 0);
			draw_cubit(mi, ' ', ' ',
				   F_[LEFT_ROTATION][1][LAST], ' ',
				   ' ', F_[TOP_ROTATION][0][1]);
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', ' ',
				   ' ', ' ',
				   ' ', F_[TOP_ROTATION][1][1]);
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', ' ',
				   ' ', F_[RIGHT_ROTATION][1][LAST],
				   ' ', F_[TOP_ROTATION][LAST][1]);
			glPopMatrix();
			if (rp->movement == FRONT_ROTATION)
				glRotatef(rp->rotatestep, 0, 0, 1);
			glTranslatef(-S1, -S1, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][0][0],
				   F_[LEFT_ROTATION][LAST][0], ' ',
				   F_[BOTTOM_ROTATION][0][LAST], ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][1][0],
				   ' ', ' ',
				   F_[BOTTOM_ROTATION][1][LAST], ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][LASTX][0],
				   ' ', F_[RIGHT_ROTATION][0][0],
				   F_[BOTTOM_ROTATION][LAST][LAST], ' ');
			glTranslatef(-S2, S1, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][0][1],
				   F_[LEFT_ROTATION][LAST][1], ' ',
				   ' ', ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][1][1],
				   ' ', ' ',
				   ' ', ' ');
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][LASTX][1],
				   ' ', F_[RIGHT_ROTATION][0][1],
				   ' ', ' ');
			glTranslatef(-S2, S1, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][0][LASTY],
				   F_[LEFT_ROTATION][LAST][LAST], ' ',
				   ' ', F_[TOP_ROTATION][0][0]);
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][1][LASTY],
				   ' ', ' ',
				   ' ', F_[TOP_ROTATION][1][0]);
			glTranslatef(S1, 0, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][LASTX][LASTY],
				   ' ', F_[RIGHT_ROTATION][0][LAST],
				   ' ', F_[TOP_ROTATION][LAST][0]);
			break;
		case LEFT_ROTATION:
		case RIGHT_ROTATION:
			glPushMatrix();
			if (rp->movement == LEFT_ROTATION)
				glRotatef(-rp->rotatestep, 1, 0, 0);
			glTranslatef(-S1, -S1, -S1);
			draw_cubit(mi, F_[BACK_ROTATION][0][LAST], ' ',
				   F_[LEFT_ROTATION][0][0], ' ',
				   F_[BOTTOM_ROTATION][0][0], ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, F_[BACK_ROTATION][0][1], ' ',
				   F_[LEFT_ROTATION][0][1], ' ',
				   ' ', ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, F_[BACK_ROTATION][0][0], ' ',
				   F_[LEFT_ROTATION][0][LAST], ' ',
				   ' ', F_[TOP_ROTATION][0][LAST]);
			glTranslatef(0, -S2, S1);
			draw_cubit(mi, ' ', ' ',
				   F_[LEFT_ROTATION][1][0], ' ',
				   F_[BOTTOM_ROTATION][0][1], ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', ' ',
				   F_[LEFT_ROTATION][1][1], ' ',
				   ' ', ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', ' ',
				   F_[LEFT_ROTATION][1][LAST], ' ',
				   ' ', F_[TOP_ROTATION][0][1]);
			glTranslatef(0, -S2, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][0][0],
				   F_[LEFT_ROTATION][LAST][0], ' ',
				   F_[BOTTOM_ROTATION][0][LAST], ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][0][1],
				   F_[LEFT_ROTATION][LAST][1], ' ',
				   ' ', ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][0][LASTY],
				   F_[LEFT_ROTATION][LAST][LAST], ' ',
				   ' ', F_[TOP_ROTATION][0][0]);
			glPopMatrix();
			glPushMatrix();
			glTranslatef(0, -S1, -S1);
			draw_cubit(mi, F_[BACK_ROTATION][1][LAST], ' ',
				   ' ', ' ',
				   F_[BOTTOM_ROTATION][1][0], ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, F_[BACK_ROTATION][1][1], ' ',
				   ' ', ' ',
				   ' ', ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, F_[BACK_ROTATION][1][0], ' ',
				   ' ', ' ',
				   ' ', F_[TOP_ROTATION][1][LAST]);
			glTranslatef(0, -S2, S1);
			draw_cubit(mi, ' ', ' ',
				   ' ', ' ',
				   F_[BOTTOM_ROTATION][1][1], ' ');
			glTranslatef(0, S2, 0);
			draw_cubit(mi, ' ', ' ',
				   ' ', ' ',
				   ' ', F_[TOP_ROTATION][1][1]);
			glTranslatef(0, -S2, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][1][0],
				   ' ', ' ',
				   F_[BOTTOM_ROTATION][1][LAST], ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][1][1],
				   ' ', ' ',
				   ' ', ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][1][LASTY],
				   ' ', ' ',
				   ' ', F_[TOP_ROTATION][1][0]);
			glPopMatrix();
			if (rp->movement == RIGHT_ROTATION)
				glRotatef(rp->rotatestep, 1, 0, 0);
			glTranslatef(S1, -S1, -S1);
			draw_cubit(mi, F_[BACK_ROTATION][LAST][LAST], ' ',
				   ' ', F_[RIGHT_ROTATION][LAST][0],
				   F_[BOTTOM_ROTATION][LAST][0], ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, F_[BACK_ROTATION][LAST][1], ' ',
				   ' ', F_[RIGHT_ROTATION][LAST][1],
				   ' ', ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, F_[BACK_ROTATION][LAST][0], ' ',
				   ' ', F_[RIGHT_ROTATION][LAST][LAST],
				   ' ', F_[TOP_ROTATION][LAST][LAST]);
			glTranslatef(0, -S2, S1);
			draw_cubit(mi, ' ', ' ',
				   ' ', F_[RIGHT_ROTATION][1][0],
				   F_[BOTTOM_ROTATION][LAST][1], ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', ' ',
				   ' ', F_[RIGHT_ROTATION][1][1],
				   ' ', ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', ' ',
				   ' ', F_[RIGHT_ROTATION][1][LAST],
				   ' ', F_[TOP_ROTATION][LAST][1]);
			glTranslatef(0, -S2, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][LASTX][0],
				   ' ', F_[RIGHT_ROTATION][0][0],
				   F_[BOTTOM_ROTATION][LAST][LAST], ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][LASTX][1],
				   ' ', F_[RIGHT_ROTATION][0][1],
				   ' ', ' ');
			glTranslatef(0, S1, 0);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][LASTX][LASTY],
				   ' ', F_[RIGHT_ROTATION][0][LAST],
				   ' ', F_[TOP_ROTATION][LAST][0]);
			break;
		case BOTTOM_ROTATION:
		case TOP_ROTATION:
			glPushMatrix();
			if (rp->movement == BOTTOM_ROTATION)
				glRotatef(-rp->rotatestep, 0, 1, 0);
			glTranslatef(-S1, -S1, -S1);
			draw_cubit(mi, F_[BACK_ROTATION][0][LAST], ' ',
				   F_[LEFT_ROTATION][0][0], ' ',
				   F_[BOTTOM_ROTATION][0][0], ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', ' ',
				   F_[LEFT_ROTATION][1][0], ' ',
				   F_[BOTTOM_ROTATION][0][1], ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][0][0],
				   F_[LEFT_ROTATION][LAST][0], ' ',
				   F_[BOTTOM_ROTATION][0][LAST], ' ');
			glTranslatef(S1, 0, -S2);
			draw_cubit(mi, F_[BACK_ROTATION][1][LAST], ' ',
				   ' ', ' ',
				   F_[BOTTOM_ROTATION][1][0], ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', ' ',
				   ' ', ' ',
				   F_[BOTTOM_ROTATION][1][1], ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][1][0],
				   ' ', ' ',
				   F_[BOTTOM_ROTATION][1][LAST], ' ');
			glTranslatef(1, 0, -S2);
			draw_cubit(mi, F_[BACK_ROTATION][LAST][LAST], ' ',
				   ' ', F_[RIGHT_ROTATION][LAST][0],
				   F_[BOTTOM_ROTATION][LAST][0], ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', ' ',
				   ' ', F_[RIGHT_ROTATION][1][0],
				   F_[BOTTOM_ROTATION][LAST][1], ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][LASTX][0],
				   ' ', F_[RIGHT_ROTATION][0][0],
				   F_[BOTTOM_ROTATION][LAST][LAST], ' ');
			glPopMatrix();
			glPushMatrix();
			glTranslatef(-S1, 0, -S1);
			draw_cubit(mi, F_[BACK_ROTATION][0][1], ' ',
				   F_[LEFT_ROTATION][0][1], ' ',
				   ' ', ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', ' ',
				   F_[LEFT_ROTATION][1][1], ' ',
				   ' ', ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][0][1],
				   F_[LEFT_ROTATION][LAST][1], ' ',
				   ' ', ' ');
			glTranslatef(1, 0, -S2);
			draw_cubit(mi, F_[BACK_ROTATION][1][1], ' ',
				   ' ', ' ',
				   ' ', ' ');
			glTranslatef(0, 0, S2);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][1][1],
				   ' ', ' ',
				   ' ', ' ');
			glTranslatef(S1, 0, -S2);
			draw_cubit(mi, F_[BACK_ROTATION][LAST][1], ' ',
				   ' ', F_[RIGHT_ROTATION][LAST][1],
				   ' ', ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', ' ',
				   ' ', F_[RIGHT_ROTATION][1][1],
				   ' ', ' ');
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][LASTX][1],
				   ' ', F_[RIGHT_ROTATION][0][1],
				   ' ', ' ');
			glPopMatrix();
			if (rp->movement == TOP_ROTATION)
				glRotatef(rp->rotatestep, 0, 1, 0);
			glTranslatef(-S1, S1, -S1);
			draw_cubit(mi, F_[BACK_ROTATION][0][0], ' ',
				   F_[LEFT_ROTATION][0][LAST], ' ',
				   ' ', F_[TOP_ROTATION][0][LAST]);
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', ' ',
				   F_[LEFT_ROTATION][1][LAST], ' ',
				   ' ', F_[TOP_ROTATION][0][1]);
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][0][LASTY],
				   F_[LEFT_ROTATION][LAST][LAST], ' ',
				   ' ', F_[TOP_ROTATION][0][0]);
			glTranslatef(S1, 0, -S2);
			draw_cubit(mi, F_[BACK_ROTATION][1][0], ' ',
				   ' ', ' ',
				   ' ', F_[TOP_ROTATION][1][LAST]);
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', ' ',
				   ' ', ' ',
				   ' ', F_[TOP_ROTATION][1][1]);
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][1][LASTY],
				   ' ', ' ',
				   ' ', F_[TOP_ROTATION][1][0]);
			glTranslatef(S1, 0, -S2);
			draw_cubit(mi, F_[BACK_ROTATION][LAST][0], ' ',
				   ' ', F_[RIGHT_ROTATION][LAST][LAST],
				   ' ', F_[TOP_ROTATION][LAST][LAST]);
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', ' ',
				   ' ', F_[RIGHT_ROTATION][1][LAST],
				   ' ', F_[TOP_ROTATION][LAST][1]);
			glTranslatef(0, 0, S1);
			draw_cubit(mi, ' ', F_[FRONT_ROTATION][LASTX][LASTY],
				   ' ', F_[RIGHT_ROTATION][0][LAST],
				   ' ', F_[TOP_ROTATION][LAST][0]);
			break;
	}
#undef S1
#undef S2
}

static void
evalmovement(ModeInfo * mi, int face, char orient)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];
	char        T1, T2, T3;

	if (face < 0 || face > 5)
		return;

	if (orient == CLOCK_WISE) {
		T1 = F_[face][0][LAST];
		T2 = F_[face][1][LAST];
		F_[face][0][LAST] = F_[face][0][0];
		F_[face][1][LAST] = F_[face][0][1];
		F_[face][0][0] = F_[face][LAST][0];
		F_[face][0][1] = F_[face][1][0];
		F_[face][1][0] = F_[face][LAST][1];
		F_[face][LAST][0] = F_[face][LAST][LAST];
		F_[face][LAST][LAST] = T1;	/* F_[face][0][LAST]; */
		F_[face][LAST][1] = T2;		/* F_[face][1][LAST]; */
	} else {
		T1 = F_[face][0][0];
		T2 = F_[face][0][1];
		F_[face][0][0] = F_[face][0][LAST];
		F_[face][0][1] = F_[face][1][LAST];
		F_[face][0][LAST] = F_[face][LAST][LAST];
		F_[face][1][LAST] = F_[face][LAST][1];
		F_[face][LAST][1] = F_[face][1][0];
		F_[face][LAST][LAST] = F_[face][LAST][0];
		F_[face][1][0] = T2;	/* F_[face][0][1]; */
		F_[face][LAST][0] = T1;		/* F_[face][0][0]; */
	}

	switch (face) {
		case BACK_ROTATION:
			if (orient == CLOCK_WISE) {
				T1 = F_[BOTTOM_ROTATION][0][0];
				T2 = F_[BOTTOM_ROTATION][1][0];
				T3 = F_[BOTTOM_ROTATION][LAST][0];
				F_[BOTTOM_ROTATION][0][0] = F_[LEFT_ROTATION][0][LAST];
				F_[BOTTOM_ROTATION][1][0] = F_[LEFT_ROTATION][0][1];
				F_[BOTTOM_ROTATION][LAST][0] = F_[LEFT_ROTATION][0][0];
				F_[LEFT_ROTATION][0][0] = F_[TOP_ROTATION][0][LAST];
				F_[LEFT_ROTATION][0][1] = F_[TOP_ROTATION][1][LAST];
				F_[LEFT_ROTATION][0][LAST] = F_[TOP_ROTATION][LAST][LAST];
				F_[TOP_ROTATION][0][LAST] = F_[RIGHT_ROTATION][LAST][LAST];
				F_[TOP_ROTATION][1][LAST] = F_[RIGHT_ROTATION][LAST][1];
				F_[TOP_ROTATION][LAST][LAST] = F_[RIGHT_ROTATION][LAST][0];
				F_[RIGHT_ROTATION][LAST][0] = T1;	/* F_[BOTTOM_ROTATION][0][0]; */
				F_[RIGHT_ROTATION][LAST][1] = T2;	/* F_[BOTTOM_ROTATION][1][0]; */
				F_[RIGHT_ROTATION][LAST][LAST] = T3;	/* F_[BOTTOM_ROTATION][LAST][0]; */
			} else {
				T1 = F_[LEFT_ROTATION][0][LAST];
				T2 = F_[LEFT_ROTATION][0][1];
				T3 = F_[LEFT_ROTATION][0][0];
				F_[LEFT_ROTATION][0][LAST] = F_[BOTTOM_ROTATION][0][0];
				F_[LEFT_ROTATION][0][1] = F_[BOTTOM_ROTATION][1][0];
				F_[LEFT_ROTATION][0][0] = F_[BOTTOM_ROTATION][LAST][0];
				F_[BOTTOM_ROTATION][0][0] = F_[RIGHT_ROTATION][LAST][0];
				F_[BOTTOM_ROTATION][1][0] = F_[RIGHT_ROTATION][LAST][1];
				F_[BOTTOM_ROTATION][LAST][0] = F_[RIGHT_ROTATION][LAST][LAST];
				F_[RIGHT_ROTATION][LAST][LAST] = F_[TOP_ROTATION][0][LAST];
				F_[RIGHT_ROTATION][LAST][1] = F_[TOP_ROTATION][1][LAST];
				F_[RIGHT_ROTATION][LAST][0] = F_[TOP_ROTATION][LAST][LAST];
				F_[TOP_ROTATION][0][LAST] = T3;		/* F_[LEFT_ROTATION][0][0]; */
				F_[TOP_ROTATION][1][LAST] = T2;		/* F_[LEFT_ROTATION][0][1]; */
				F_[TOP_ROTATION][LAST][LAST] = T1;	/* F_[LEFT_ROTATION][0][LAST]; */
			}
			break;
		case FRONT_ROTATION:
			if (orient == CLOCK_WISE) {
				T1 = F_[RIGHT_ROTATION][0][LAST];
				T2 = F_[RIGHT_ROTATION][0][1];
				T3 = F_[RIGHT_ROTATION][0][0];
				F_[RIGHT_ROTATION][0][LAST] = F_[TOP_ROTATION][0][0];
				F_[RIGHT_ROTATION][0][1] = F_[TOP_ROTATION][1][0];
				F_[RIGHT_ROTATION][0][0] = F_[TOP_ROTATION][LAST][0];
				F_[TOP_ROTATION][0][0] = F_[LEFT_ROTATION][LAST][0];
				F_[TOP_ROTATION][1][0] = F_[LEFT_ROTATION][LAST][1];
				F_[TOP_ROTATION][LAST][0] = F_[LEFT_ROTATION][LAST][LAST];
				F_[LEFT_ROTATION][LAST][LAST] = F_[BOTTOM_ROTATION][0][LAST];
				F_[LEFT_ROTATION][LAST][1] = F_[BOTTOM_ROTATION][1][LAST];
				F_[LEFT_ROTATION][LAST][0] = F_[BOTTOM_ROTATION][LAST][LAST];
				F_[BOTTOM_ROTATION][0][LAST] = T3;	/* F_[RIGHT_ROTATION][0][0]; */
				F_[BOTTOM_ROTATION][1][LAST] = T2;	/* F_[RIGHT_ROTATION][0][1]; */
				F_[BOTTOM_ROTATION][LAST][LAST] = T1;	/* F_[RIGHT_ROTATION][0][LAST]; */
			} else {
				T1 = F_[TOP_ROTATION][0][0];
				T2 = F_[TOP_ROTATION][1][0];
				T3 = F_[TOP_ROTATION][LAST][0];
				F_[TOP_ROTATION][0][0] = F_[RIGHT_ROTATION][0][LAST];
				F_[TOP_ROTATION][1][0] = F_[RIGHT_ROTATION][0][1];
				F_[TOP_ROTATION][LAST][0] = F_[RIGHT_ROTATION][0][0];
				F_[RIGHT_ROTATION][0][0] = F_[BOTTOM_ROTATION][0][LAST];
				F_[RIGHT_ROTATION][0][1] = F_[BOTTOM_ROTATION][1][LAST];
				F_[RIGHT_ROTATION][0][LAST] = F_[BOTTOM_ROTATION][LAST][LAST];
				F_[BOTTOM_ROTATION][0][LAST] = F_[LEFT_ROTATION][LAST][LAST];
				F_[BOTTOM_ROTATION][1][LAST] = F_[LEFT_ROTATION][LAST][1];
				F_[BOTTOM_ROTATION][LAST][LAST] = F_[LEFT_ROTATION][LAST][0];
				F_[LEFT_ROTATION][LAST][0] = T1;	/* F_[TOP_ROTATION][0][0]; */
				F_[LEFT_ROTATION][LAST][1] = T2;	/* F_[TOP_ROTATION][1][0]; */
				F_[LEFT_ROTATION][LAST][LAST] = T3;	/* F_[TOP_ROTATION][LAST][0]; */
			}
			break;
		case LEFT_ROTATION:
			if (orient == CLOCK_WISE) {
				T1 = F_[TOP_ROTATION][0][0];
				T2 = F_[TOP_ROTATION][0][1];
				T3 = F_[TOP_ROTATION][0][LAST];
				F_[TOP_ROTATION][0][0] = F_[BACK_ROTATION][0][0];
				F_[TOP_ROTATION][0][1] = F_[BACK_ROTATION][0][1];
				F_[TOP_ROTATION][0][LAST] = F_[BACK_ROTATION][0][LAST];
				F_[BACK_ROTATION][0][0] = F_[BOTTOM_ROTATION][0][0];
				F_[BACK_ROTATION][0][1] = F_[BOTTOM_ROTATION][0][1];
				F_[BACK_ROTATION][0][LAST] = F_[BOTTOM_ROTATION][0][LAST];
				F_[BOTTOM_ROTATION][0][0] = F_[FRONT_ROTATION][0][0];
				F_[BOTTOM_ROTATION][0][1] = F_[FRONT_ROTATION][0][1];
				F_[BOTTOM_ROTATION][0][LAST] = F_[FRONT_ROTATION][0][LASTY];
				F_[FRONT_ROTATION][0][0] = T1;	/* F_[TOP_ROTATION][0][0]; */
				F_[FRONT_ROTATION][0][1] = T2;	/* F_[TOP_ROTATION][0][1]; */
				F_[FRONT_ROTATION][0][LASTY] = T3;	/* F_[TOP_ROTATION][0][LAST]; */
			} else {
				T1 = F_[BACK_ROTATION][0][0];
				T2 = F_[BACK_ROTATION][0][1];
				T3 = F_[BACK_ROTATION][0][LAST];
				F_[BACK_ROTATION][0][0] = F_[TOP_ROTATION][0][0];
				F_[BACK_ROTATION][0][1] = F_[TOP_ROTATION][0][1];
				F_[BACK_ROTATION][0][LAST] = F_[TOP_ROTATION][0][LAST];
				F_[TOP_ROTATION][0][0] = F_[FRONT_ROTATION][0][0];
				F_[TOP_ROTATION][0][1] = F_[FRONT_ROTATION][0][1];
				F_[TOP_ROTATION][0][LAST] = F_[FRONT_ROTATION][0][LASTY];
				F_[FRONT_ROTATION][0][0] = F_[BOTTOM_ROTATION][0][0];
				F_[FRONT_ROTATION][0][1] = F_[BOTTOM_ROTATION][0][1];
				F_[FRONT_ROTATION][0][LASTY] = F_[BOTTOM_ROTATION][0][LAST];
				F_[BOTTOM_ROTATION][0][0] = T1;		/* F_[BACK_ROTATION][0][0]; */
				F_[BOTTOM_ROTATION][0][1] = T2;		/* F_[BACK_ROTATION][0][1]; */
				F_[BOTTOM_ROTATION][0][LAST] = T3;	/* F_[BACK_ROTATION][0][LAST]; */
			}
			break;
		case RIGHT_ROTATION:
			if (orient == CLOCK_WISE) {
				T1 = F_[TOP_ROTATION][LAST][0];
				T2 = F_[TOP_ROTATION][LAST][1];
				T3 = F_[TOP_ROTATION][LAST][LAST];
				F_[TOP_ROTATION][LAST][0] = F_[FRONT_ROTATION][LASTX][0];
				F_[TOP_ROTATION][LAST][1] = F_[FRONT_ROTATION][LASTX][1];
				F_[TOP_ROTATION][LAST][LAST] = F_[FRONT_ROTATION][LASTX][LASTY];
				F_[FRONT_ROTATION][LASTX][0] = F_[BOTTOM_ROTATION][LAST][0];
				F_[FRONT_ROTATION][LASTX][1] = F_[BOTTOM_ROTATION][LAST][1];
				F_[FRONT_ROTATION][LASTX][LASTY] = F_[BOTTOM_ROTATION][LAST][LAST];
				F_[BOTTOM_ROTATION][LAST][0] = F_[BACK_ROTATION][LAST][0];
				F_[BOTTOM_ROTATION][LAST][1] = F_[BACK_ROTATION][LAST][1];
				F_[BOTTOM_ROTATION][LAST][LAST] = F_[BACK_ROTATION][LAST][LAST];
				F_[BACK_ROTATION][LAST][0] = T1;	/* F_[TOP_ROTATION][LAST][0]; */
				F_[BACK_ROTATION][LAST][1] = T2;	/* F_[TOP_ROTATION][LAST][1]; */
				F_[BACK_ROTATION][LAST][LAST] = T3;	/* F_[TOP_ROTATION][LAST][LAST]; */
			} else {
				T1 = F_[FRONT_ROTATION][LASTX][0];
				T2 = F_[FRONT_ROTATION][LASTX][1];
				T3 = F_[FRONT_ROTATION][LASTX][LASTY];
				F_[FRONT_ROTATION][LASTX][0] = F_[TOP_ROTATION][LAST][0];
				F_[FRONT_ROTATION][LASTX][1] = F_[TOP_ROTATION][LAST][1];
				F_[FRONT_ROTATION][LASTX][LASTY] = F_[TOP_ROTATION][LAST][LAST];
				F_[TOP_ROTATION][LAST][0] = F_[BACK_ROTATION][LAST][0];
				F_[TOP_ROTATION][LAST][1] = F_[BACK_ROTATION][LAST][1];
				F_[TOP_ROTATION][LAST][LAST] = F_[BACK_ROTATION][LAST][LAST];
				F_[BACK_ROTATION][LAST][0] = F_[BOTTOM_ROTATION][LAST][0];
				F_[BACK_ROTATION][LAST][1] = F_[BOTTOM_ROTATION][LAST][1];
				F_[BACK_ROTATION][LAST][LAST] = F_[BOTTOM_ROTATION][LAST][LAST];
				F_[BOTTOM_ROTATION][LAST][0] = T1;	/* F_[FRONT_ROTATION][LASTX][0]; */
				F_[BOTTOM_ROTATION][LAST][1] = T2;	/* F_[FRONT_ROTATION][LASTX][1]; */
				F_[BOTTOM_ROTATION][LAST][LAST] = T3;	/* F_[FRONT_ROTATION][LASTX][LASTY]; */
			}
			break;
		case BOTTOM_ROTATION:
			if (orient == CLOCK_WISE) {
				T1 = F_[FRONT_ROTATION][0][0];
				T2 = F_[FRONT_ROTATION][1][0];
				T3 = F_[FRONT_ROTATION][LASTX][0];
				F_[FRONT_ROTATION][0][0] = F_[LEFT_ROTATION][0][0];
				F_[FRONT_ROTATION][1][0] = F_[LEFT_ROTATION][1][0];
				F_[FRONT_ROTATION][LASTX][0] = F_[LEFT_ROTATION][LAST][0];
				F_[LEFT_ROTATION][0][0] = F_[BACK_ROTATION][LAST][LAST];
				F_[LEFT_ROTATION][1][0] = F_[BACK_ROTATION][1][LAST];
				F_[LEFT_ROTATION][LAST][0] = F_[BACK_ROTATION][0][LAST];
				F_[BACK_ROTATION][LAST][LAST] = F_[RIGHT_ROTATION][0][0];
				F_[BACK_ROTATION][1][LAST] = F_[RIGHT_ROTATION][1][0];
				F_[BACK_ROTATION][0][LAST] = F_[RIGHT_ROTATION][LAST][0];
				F_[RIGHT_ROTATION][0][0] = T1;	/* F_[FRONT_ROTATION][0][0]; */
				F_[RIGHT_ROTATION][1][0] = T2;	/* F_[FRONT_ROTATION][1][0]; */
				F_[RIGHT_ROTATION][LAST][0] = T3;	/* F_[FRONT_ROTATION][LASTX][0]; */
			} else {
				T1 = F_[BACK_ROTATION][LAST][LAST];
				T2 = F_[BACK_ROTATION][1][LAST];
				T3 = F_[BACK_ROTATION][0][LAST];
				F_[BACK_ROTATION][LAST][LAST] = F_[LEFT_ROTATION][0][0];
				F_[BACK_ROTATION][1][LAST] = F_[LEFT_ROTATION][1][0];
				F_[BACK_ROTATION][0][LAST] = F_[LEFT_ROTATION][LAST][0];
				F_[LEFT_ROTATION][0][0] = F_[FRONT_ROTATION][0][0];
				F_[LEFT_ROTATION][1][0] = F_[FRONT_ROTATION][1][0];
				F_[LEFT_ROTATION][LAST][0] = F_[FRONT_ROTATION][LASTX][0];
				F_[FRONT_ROTATION][0][0] = F_[RIGHT_ROTATION][0][0];
				F_[FRONT_ROTATION][1][0] = F_[RIGHT_ROTATION][1][0];
				F_[FRONT_ROTATION][LASTX][0] = F_[RIGHT_ROTATION][LAST][0];
				F_[RIGHT_ROTATION][0][0] = T1;	/* F_[BACK_ROTATION][LAST][LAST]; */
				F_[RIGHT_ROTATION][1][0] = T2;	/* F_[BACK_ROTATION][1][LAST]; */
				F_[RIGHT_ROTATION][LAST][0] = T3;	/* F_[BACK_ROTATION][0][LAST]; */
			}
			break;
		case TOP_ROTATION:
			if (orient == CLOCK_WISE) {
				T1 = F_[BACK_ROTATION][0][0];
				T2 = F_[BACK_ROTATION][1][0];
				T3 = F_[BACK_ROTATION][LAST][0];
				F_[BACK_ROTATION][0][0] = F_[LEFT_ROTATION][LAST][LAST];
				F_[BACK_ROTATION][1][0] = F_[LEFT_ROTATION][1][LAST];
				F_[BACK_ROTATION][LAST][0] = F_[LEFT_ROTATION][0][LAST];
				F_[LEFT_ROTATION][LAST][LAST] = F_[FRONT_ROTATION][LASTX][LASTY];
				F_[LEFT_ROTATION][1][LAST] = F_[FRONT_ROTATION][1][LASTY];
				F_[LEFT_ROTATION][0][LAST] = F_[FRONT_ROTATION][0][LASTY];
				F_[FRONT_ROTATION][LASTX][LASTY] = F_[RIGHT_ROTATION][LAST][LAST];
				F_[FRONT_ROTATION][1][LASTY] = F_[RIGHT_ROTATION][1][LAST];
				F_[FRONT_ROTATION][0][LASTY] = F_[RIGHT_ROTATION][0][LAST];
				F_[RIGHT_ROTATION][LAST][LAST] = T1;	/* F_[BACK_ROTATION][0][0]; */
				F_[RIGHT_ROTATION][1][LAST] = T2;	/* F_[BACK_ROTATION][1][0]; */
				F_[RIGHT_ROTATION][0][LAST] = T3;	/* F_[BACK_ROTATION][LAST][0]; */
			} else {
				T1 = F_[RIGHT_ROTATION][LAST][LAST];
				T2 = F_[RIGHT_ROTATION][1][LAST];
				T3 = F_[RIGHT_ROTATION][0][LAST];
				F_[RIGHT_ROTATION][LAST][LAST] = F_[FRONT_ROTATION][LASTX][LASTY];
				F_[RIGHT_ROTATION][1][LAST] = F_[FRONT_ROTATION][1][LASTY];
				F_[RIGHT_ROTATION][0][LAST] = F_[FRONT_ROTATION][0][LASTY];
				F_[FRONT_ROTATION][LASTX][LASTY] = F_[LEFT_ROTATION][LAST][LAST];
				F_[FRONT_ROTATION][1][LASTY] = F_[LEFT_ROTATION][1][LAST];
				F_[FRONT_ROTATION][0][LASTY] = F_[LEFT_ROTATION][0][LAST];
				F_[LEFT_ROTATION][LAST][LAST] = F_[BACK_ROTATION][0][0];
				F_[LEFT_ROTATION][1][LAST] = F_[BACK_ROTATION][1][0];
				F_[LEFT_ROTATION][0][LAST] = F_[BACK_ROTATION][LAST][0];
				F_[BACK_ROTATION][0][0] = T1;	/* F_[RIGHT_ROTATION][LAST][LAST]; */
				F_[BACK_ROTATION][1][0] = T2;	/* F_[RIGHT_ROTATION][1][LAST]; */
				F_[BACK_ROTATION][LAST][0] = T3;	/* F_[RIGHT_ROTATION][0][LAST]; */
			}
			break;
	}
}

static void
shuffle(ModeInfo * mi)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];
	int         i, mov, ori;

	memset(F_[TOP_ROTATION], 'R', MAXSIZEXZ);
	memset(F_[LEFT_ROTATION], 'Y', MAXSIZEZY);
	memset(F_[FRONT_ROTATION], 'W', MAXSIZEXY);
	memset(F_[RIGHT_ROTATION], 'G', MAXSIZEZY);
	memset(F_[BOTTOM_ROTATION], 'O', MAXSIZEXZ);
	memset(F_[BACK_ROTATION], 'B', MAXSIZEXY);

	rp->storedmoves = MI_BATCHCOUNT(mi);
	if (rp->storedmoves < 0) {
		if (rp->movedfaces != NULL)
			(void) free((void *) rp->movedfaces);
		if (rp->movedorient != NULL)
			(void) free((void *) rp->movedorient);
		rp->movedfaces = NULL;
		rp->movedorient = NULL;
		rp->storedmoves = NRAND(-rp->storedmoves) + 1;
	}
	if ((rp->storedmoves) && (rp->movedfaces == NULL))
		if ((rp->movedfaces =
		(char *) calloc(rp->storedmoves + 1, sizeof (char))) == NULL)
			            (void) fprintf(stderr,
			"Could not allocate memory for rubik move buffer\n");

	if ((rp->storedmoves) && (rp->movedorient == NULL))
		if ((rp->movedorient =
		(char *) calloc(rp->storedmoves + 1, sizeof (char))) == NULL)
			            (void) fprintf(stderr,
						   "Could not allocate memory for rubik orient buffer\n");

	if (MI_CYCLES(mi) <= 1) {
		rp->anglestep = 90.0;
	} else {
		rp->anglestep = 90.0 / (GLfloat) (MI_CYCLES(mi));
	}

	for (i = 0; i < rp->storedmoves; i++) {
		int         condition;

		do {
			mov = NRAND(6);
			ori = NRAND(2);
			condition = 1;
			if (i > 0)	/* avoid immediate undoing moves */
				if (mov == rp->movedfaces[i - 1] &&
				    ori == rp->movedorient[i - 1])
					condition = 0;
			if (i > 1)	/* avoid 3 consecutive identical moves */
				if (mov == rp->movedfaces[i - 1] &&
				    ori != rp->movedorient[i - 1] &&
				    rp->movedfaces[i - 1] == rp->movedfaces[i - 2] &&
				    rp->movedorient[i - 1] == rp->movedorient[i - 2])
					condition = 0;
		} while (!condition);
		if (hideshuffling)
			evalmovement(mi, mov, ori);
		rp->movedfaces[i] = mov;
		rp->movedorient[i] = (ori == CLOCK_WISE) ? C_CLOCK_WISE : CLOCK_WISE;
	}
	rp->movement = NO_ROTATION;
	rp->rotatestep = 0;
	rp->action = hideshuffling ? ACTION_SOLVE : ACTION_SHUFFLE;
	rp->shufflingmoves = 0;
	rp->done = 0;
}

void
draw_rubik(ModeInfo * mi)
{
	rubikstruct *rp = &rubik[MI_SCREEN(mi)];

	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	glDrawBuffer(GL_BACK);
	glXMakeCurrent(display, window, rp->glx_context);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	glTranslatef(0.0, 0.0, -10.0);

	if (!MI_WIN_IS_ICONIC(mi)) {
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
				rp->movement = NO_ROTATION;
				rp->rotatestep = 0;
				rp->action = ACTION_SOLVE;
				rp->done = 0;
			}
		} else {
			if (rp->movement == NO_ROTATION) {
				if (rp->shufflingmoves < rp->storedmoves) {
					rp->rotatestep = 0;
					rp->movement = rp->movedfaces[rp->shufflingmoves];
					rp->orientation =
						(rp->movedorient[rp->shufflingmoves] == CLOCK_WISE) ?
						C_CLOCK_WISE : CLOCK_WISE;
				} else {
					rp->rotatestep = 0;
					rp->done = 1;
				}
			} else {
				rp->rotatestep += (rp->orientation == CLOCK_WISE) ?
					-rp->anglestep : rp->anglestep;
				if (rp->rotatestep > 90 || rp->rotatestep < -90) {
					evalmovement(mi, rp->movement, rp->orientation);
					rp->shufflingmoves++;
					rp->movement = NO_ROTATION;
				}
			}
		}
	} else {
		if (rp->done) {
			if (++rp->rotatestep > DELAY_AFTER_SOLVING)
				shuffle(mi);
		} else {
			if (rp->movement == NO_ROTATION) {
				if (rp->storedmoves > 0) {
					rp->rotatestep = 0;
					rp->movement = rp->movedfaces[rp->storedmoves - 1];
					rp->orientation = rp->movedorient[rp->storedmoves - 1];
				} else {
					rp->rotatestep = 0;
					rp->done = 1;
				}
			} else {
				rp->rotatestep += (rp->orientation == CLOCK_WISE) ?
					-rp->anglestep : rp->anglestep;
				if (rp->rotatestep > 90 || rp->rotatestep < -90) {
					evalmovement(mi, rp->movement, rp->orientation);
					rp->storedmoves--;
					rp->movement = NO_ROTATION;
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

	rp->glx_context = init_GL(mi);

	reshape(mi, MI_WIN_WIDTH(mi), MI_WIN_HEIGHT(mi));
	objects = glGenLists(1);
	pinit(mi);
}

void
change_rubik(ModeInfo * mi)
{
	pinit(mi);
}

void
release_rubik(ModeInfo * mi)
{
	if (rubik != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			rubikstruct *rp = &rubik[screen];

			if (rp->movedfaces != NULL)
				(void) free((void *) rp->movedfaces);
			if (rp->movedorient != NULL)
				(void) free((void *) rp->movedorient);
		}
		(void) free((void *) rubik);
		rubik = NULL;
	}
}

#undef F_
#endif
