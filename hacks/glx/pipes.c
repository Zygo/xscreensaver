/* -*- Mode: C; tab-width: 4 -*- */
/* pipes --- 3D selfbuiding pipe system */

#if 0
static const char sccsid[] = "@(#)pipes.c	4.07 97/11/24 xlockmore";
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
 * This program was inspired on a WindowsNT(R)'s screen saver. It was written 
 * from scratch and it was not based on any other source code.
 *
 * ==========================================================================
 * The routine myElbow is derivated from the doughnut routine from the MesaGL
 * library (more especifically the Mesaaux library) written by Brian Paul.
 * ==========================================================================
 *
 * Thanks goes to Brian Paul for making it possible and inexpensive to use
 * OpenGL at home.
 *
 * Since I'm not a native English speaker, my apologies for any grammatical
 * mistake.
 *
 * My e-mail address is
 * m-vianna@usa.net
 * Marcelo F. Vianna (Apr-09-1997)
 *
 * Revision History:
 * 24-Jun-12: Eliminate single-buffer dependency.
 * 29-Apr-97: Factory equipment by Ed Mackey.  Productive day today, eh?
 * 29-Apr-97: Less tight turns Jeff Epler <jepler@inetnebr.com>
 * 29-Apr-97: Efficiency speed-ups by Marcelo F. Vianna
 */

/* This program was originally written to be single-buffered: it kept
   building up new objects in the front buffer by never clearing the
   depth or color buffers at the end of each frame.  In that way, it
   was drawing a very small number of polygons per frame.  However,
   modern systems make it difficult to live in a single-buffered world
   like that.  So I changed it to re-generate the scene at every
   frame, which makes it vastly less efficient, but also, makes it
   work right on modern hardware.  It generates the entire system up
   front, putting each "frame" of the animation into its own display
   list; then it draws successively more of those display lists each
   time the redisplay method is called.  When it reaches the end,
   it regenerates a new system and re-populates the existing display
   lists. -- jwz.
 */

#ifdef STANDALONE
# define DEFAULTS	"*delay:		10000   \n"			\
					"*count:		2       \n"			\
					"*cycles:		5       \n"			\
					"*size:			500     \n"			\
	               	"*showFPS:      False   \n"		    \
	               	"*fpsSolid:     True    \n"		    \
	               	"*wireframe:    False   \n"		    \
					"*suppressRotationAnimation: True\n" \

# define release_pipes 0
# include "xlockmore.h"				/* from the xscreensaver distribution */
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

#ifdef USE_GL

#include "sphere.h"
#include "buildlwo.h"
#include "teapot.h"
#include "gltrackball.h"

#define DEF_FACTORY     "2"
#define DEF_FISHEYE     "True"
#define DEF_TIGHTTURNS  "False"
#define DEF_ROTATEPIPES "True"
#define NofSysTypes     3

static int  factory;
static Bool fisheye, tightturns, rotatepipes;

static XrmOptionDescRec opts[] =
{
	{"-factory", ".pipes.factory", XrmoptionSepArg, 0},
	{"-fisheye", ".pipes.fisheye", XrmoptionNoArg, "on"},
	{"+fisheye", ".pipes.fisheye", XrmoptionNoArg, "off"},
	{"-tightturns", ".pipes.tightturns", XrmoptionNoArg, "on"},
	{"+tightturns", ".pipes.tightturns", XrmoptionNoArg, "off"},
      {"-rotatepipes", ".pipes.rotatepipes", XrmoptionNoArg, "on"},
      {"+rotatepipes", ".pipes.rotatepipes", XrmoptionNoArg, "off"},
};
static argtype vars[] =
{
	{&factory, "factory", "Factory", DEF_FACTORY, t_Int},
	{&fisheye, "fisheye", "Fisheye", DEF_FISHEYE, t_Bool},
	{&tightturns, "tightturns", "Tightturns", DEF_TIGHTTURNS, t_Bool},
	{&rotatepipes, "rotatepipes", "Rotatepipes", DEF_ROTATEPIPES, t_Bool},
};
static OptionStruct desc[] =
{
	{"-factory num", "how much extra equipment in pipes (0 for none)"},
	{"-/+fisheye", "turn on/off zoomed-in view of pipes"},
	{"-/+tightturns", "turn on/off tight turns"},
	{"-/+rotatepipes", "turn on/off pipe system rotation per screenful"},
};

ENTRYPOINT ModeSpecOpt pipes_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   pipes_description =
{"pipes", "init_pipes", "draw_pipes", NULL,
 "draw_pipes",
 "change_pipes", "free_pipes", &pipes_opts,
 1000, 2, 5, 500, 4, 1.0, "",
 "Shows a selfbuilding pipe system", 0, NULL};

#endif

#define Scale4Window               0.1

#define one_third                  0.3333333333333333333

#define dirNone -1
#define dirUP 0
#define dirDOWN 1
#define dirLEFT 2
#define dirRIGHT 3
#define dirNEAR 4
#define dirFAR 5

#define HCELLS 33
#define VCELLS 25
#define DEFINEDCOLORS 7
#define elbowradius 0.5

/*************************************************************************/

typedef struct {
	int         flip;

	int         Cells[HCELLS][VCELLS][HCELLS];
	int         usedcolors[DEFINEDCOLORS];
	int         directions[6];
	int         ndirections;
	int         nowdir, olddir;
	int         system_number;
	int         counter;
	int         PX, PY, PZ;
	int         number_of_systems;
	int         system_type;
	int         system_length;
	int         turncounter;
	Window      window;
	const float *system_color;
	GLfloat     initial_rotation;
	GLuint      valve, bolts, betweenbolts, elbowbolts, elbowcoins;
	GLuint      guagehead, guageface, guagedial, guageconnector, teapot;
    int         teapot_polys;
	GLXContext *glx_context;

    Bool button_down_p;
   trackball_state *trackball;
    GLuint *dlists, *poly_counts;
    int dlist_count, dlist_size;
    int system_index, system_size;

    int fadeout;

} pipesstruct;

extern struct lwo LWO_BigValve, LWO_PipeBetweenBolts, LWO_Bolts3D;
extern struct lwo LWO_GuageHead, LWO_GuageFace, LWO_GuageDial, LWO_GuageConnector;
extern struct lwo LWO_ElbowBolts, LWO_ElbowCoins;

static const float front_shininess[] = {60.0};
static const float front_specular[] = {0.7, 0.7, 0.7, 1.0};
static const float ambient0[] = {0.4, 0.4, 0.4, 1.0};
static const float diffuse0[] = {1.0, 1.0, 1.0, 1.0};
static const float ambient1[] = {0.2, 0.2, 0.2, 1.0};
static const float diffuse1[] = {0.5, 0.5, 0.5, 1.0};
static const float position0[] = {1.0, 1.0, 1.0, 0.0};
static const float position1[] = {-1.0, -1.0, 1.0, 0.0};
static const float lmodel_ambient[] = {0.5, 0.5, 0.5, 1.0};
static const float lmodel_twoside[] = {GL_TRUE};

static const float MaterialRed[] = {0.7, 0.0, 0.0, 1.0};
static const float MaterialGreen[] = {0.1, 0.5, 0.2, 1.0};
static const float MaterialBlue[] = {0.0, 0.0, 0.7, 1.0};
static const float MaterialCyan[] = {0.2, 0.5, 0.7, 1.0};
static const float MaterialYellow[] = {0.7, 0.7, 0.0, 1.0};
static const float MaterialMagenta[] = {0.6, 0.2, 0.5, 1.0};
static const float MaterialWhite[] = {0.7, 0.7, 0.7, 1.0};
static const float MaterialGray[] = {0.2, 0.2, 0.2, 1.0};

static pipesstruct *pipes = NULL;


static void
MakeTube(ModeInfo *mi, int direction)
{
    Bool        wire = MI_IS_WIREFRAME(mi);
	float       an;
	float       SINan_3, COSan_3;
    int facets = (wire ? 5 : 24);

	/*dirUP    = 00000000 */
	/*dirDOWN  = 00000001 */
	/*dirLEFT  = 00000010 */
	/*dirRIGHT = 00000011 */
	/*dirNEAR  = 00000100 */
	/*dirFAR   = 00000101 */

	if (!(direction & 4)) {
		glRotatef(90.0, (direction & 2) ? 0.0 : 1.0,
			  (direction & 2) ? 1.0 : 0.0, 0.0);
	}
	glBegin(wire ? GL_LINE_STRIP : GL_QUAD_STRIP);
	for (an = 0.0; an <= 2.0 * M_PI; an += M_PI * 2 / facets) {
		glNormal3f((COSan_3 = cos(an) / 3.0), (SINan_3 = sin(an) / 3.0), 0.0);
		glVertex3f(COSan_3, SINan_3, one_third);
		glVertex3f(COSan_3, SINan_3, -one_third);
        mi->polygon_count++;
	}
	glEnd();
}

static void
mySphere(float radius, Bool wire)
{
#if 0
	GLUquadricObj *quadObj;

	quadObj = gluNewQuadric();
	gluQuadricDrawStyle(quadObj, (GLenum) GLU_FILL);
	gluSphere(quadObj, radius, 16, 16);
	gluDeleteQuadric(quadObj);
#else
    glPushMatrix();
    glScalef (radius, radius, radius);
    glRotatef (90, 1, 0, 0);
    unit_sphere (16, 16, wire);
    glPopMatrix();
#endif
}

static void
myElbow(ModeInfo * mi, int bolted)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];
    Bool        wire = MI_IS_WIREFRAME(mi);

    int nsides = (wire ? 6 : 25);
    int rings  = nsides;
#define r one_third
#define R one_third

	int         i, j;
	GLfloat     p0[3], p1[3], p2[3], p3[3];
	GLfloat     n0[3], n1[3], n2[3], n3[3];
	GLfloat     COSphi, COSphi1, COStheta, COStheta1;
	GLfloat     _SINtheta, _SINtheta1;

	for (i = 0; i <= rings / 4; i++) {
		GLfloat     theta, theta1;

		theta = (GLfloat) i *2.0 * M_PI / rings;

		theta1 = (GLfloat) (i + 1) * 2.0 * M_PI / rings;
		for (j = 0; j < nsides; j++) {
			GLfloat     phi, phi1;

			phi = (GLfloat) j *2.0 * M_PI / nsides;

			phi1 = (GLfloat) (j + 1) * 2.0 * M_PI / nsides;

			p0[0] = (COStheta = cos(theta)) * (R + r * (COSphi = cos(phi)));
			p0[1] = (_SINtheta = -sin(theta)) * (R + r * COSphi);

			p1[0] = (COStheta1 = cos(theta1)) * (R + r * COSphi);
			p1[1] = (_SINtheta1 = -sin(theta1)) * (R + r * COSphi);

			p2[0] = COStheta1 * (R + r * (COSphi1 = cos(phi1)));
			p2[1] = _SINtheta1 * (R + r * COSphi1);

			p3[0] = COStheta * (R + r * COSphi1);
			p3[1] = _SINtheta * (R + r * COSphi1);

			n0[0] = COStheta * COSphi;
			n0[1] = _SINtheta * COSphi;

			n1[0] = COStheta1 * COSphi;
			n1[1] = _SINtheta1 * COSphi;

			n2[0] = COStheta1 * COSphi1;
			n2[1] = _SINtheta1 * COSphi1;

			n3[0] = COStheta * COSphi1;
			n3[1] = _SINtheta * COSphi1;

			p0[2] = p1[2] = r * (n0[2] = n1[2] = sin(phi));
			p2[2] = p3[2] = r * (n2[2] = n3[2] = sin(phi1));

			glBegin(wire ? GL_LINE_LOOP : GL_QUADS);
			glNormal3fv(n3);
			glVertex3fv(p3);
			glNormal3fv(n2);
			glVertex3fv(p2);
			glNormal3fv(n1);
			glVertex3fv(p1);
			glNormal3fv(n0);
			glVertex3fv(p0);
            mi->polygon_count++;
			glEnd();
		}
	}

	if (factory > 0 && bolted) {
		/* Bolt the elbow onto the pipe system */
		glFrontFace(GL_CW);
		glPushMatrix();
		glRotatef(90.0, 0.0, 0.0, -1.0);
		glRotatef(90.0, 0.0, 1.0, 0.0);
		glTranslatef(0.0, one_third, one_third);
		glCallList(pp->elbowcoins);
        mi->polygon_count += LWO_ElbowCoins.num_pnts/3;
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
		glCallList(pp->elbowbolts);
        mi->polygon_count += LWO_ElbowBolts.num_pnts/3;
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pp->system_color);
		glPopMatrix();
		glFrontFace(GL_CCW);
	}
#undef r
#undef R
#undef nsides
#undef rings
}

static void
FindNeighbors(ModeInfo * mi)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];

	pp->ndirections = 0;
	pp->directions[dirUP] = (!pp->Cells[pp->PX][pp->PY + 1][pp->PZ]) ? 1 : 0;
	pp->ndirections += pp->directions[dirUP];
	pp->directions[dirDOWN] = (!pp->Cells[pp->PX][pp->PY - 1][pp->PZ]) ? 1 : 0;
	pp->ndirections += pp->directions[dirDOWN];
	pp->directions[dirLEFT] = (!pp->Cells[pp->PX - 1][pp->PY][pp->PZ]) ? 1 : 0;
	pp->ndirections += pp->directions[dirLEFT];
	pp->directions[dirRIGHT] = (!pp->Cells[pp->PX + 1][pp->PY][pp->PZ]) ? 1 : 0;
	pp->ndirections += pp->directions[dirRIGHT];
	pp->directions[dirFAR] = (!pp->Cells[pp->PX][pp->PY][pp->PZ - 1]) ? 1 : 0;
	pp->ndirections += pp->directions[dirFAR];
	pp->directions[dirNEAR] = (!pp->Cells[pp->PX][pp->PY][pp->PZ + 1]) ? 1 : 0;
	pp->ndirections += pp->directions[dirNEAR];
}

static int
SelectNeighbor(ModeInfo * mi)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];
	int         dirlist[6];
	int         i, j;

	for (i = 0, j = 0; i < 6; i++) {
		if (pp->directions[i]) {
			dirlist[j] = i;
			j++;
		}
	}

	return dirlist[NRAND(pp->ndirections)];
}

static void
MakeValve(ModeInfo * mi, int newdir)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];

	/* There is a glPopMatrix() right after this subroutine returns. */
	switch (newdir) {
		case dirUP:
		case dirDOWN:
			glRotatef(90.0, 1.0, 0.0, 0.0);
			glRotatef(NRAND(3) * 90.0, 0.0, 0.0, 1.0);
			break;
		case dirLEFT:
		case dirRIGHT:
			glRotatef(90.0, 0.0, -1.0, 0.0);
			glRotatef((NRAND(3) * 90.0) - 90.0, 0.0, 0.0, 1.0);
			break;
		case dirNEAR:
		case dirFAR:
			glRotatef(NRAND(4) * 90.0, 0.0, 0.0, 1.0);
			break;
	}
	glFrontFace(GL_CW);
	glCallList(pp->betweenbolts);
    mi->polygon_count += LWO_PipeBetweenBolts.num_pnts/3;
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
	glCallList(pp->bolts);
    mi->polygon_count += LWO_Bolts3D.num_pnts/3;
	if (!MI_IS_MONO(mi)) {
		if (pp->system_color == MaterialRed) {
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, NRAND(2) ? MaterialYellow : MaterialBlue);
		} else if (pp->system_color == MaterialBlue) {
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, NRAND(2) ? MaterialRed : MaterialYellow);
		} else if (pp->system_color == MaterialYellow) {
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, NRAND(2) ? MaterialBlue : MaterialRed);
		} else {
			switch ((NRAND(3))) {
				case 0:
					glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialRed);
					break;
				case 1:
					glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialBlue);
					break;
				case 2:
					glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialYellow);
			}
		}
	}
	glRotatef((GLfloat) (NRAND(90)), 1.0, 0.0, 0.0);
	glCallList(pp->valve);
    mi->polygon_count += LWO_BigValve.num_pnts/3;
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pp->system_color);
	glFrontFace(GL_CCW);
}

static int
MakeGuage(ModeInfo * mi, int newdir)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];

	/* Can't have a guage on a vertical pipe. */
	if ((newdir == dirUP) || (newdir == dirDOWN))
		return (0);

	/* Is there space above this pipe for a guage? */
	if (!pp->directions[dirUP])
		return (0);

	/* Yes!  Mark the space as used. */
	pp->Cells[pp->PX][pp->PY + 1][pp->PZ] = 1;

	glFrontFace(GL_CW);
	glPushMatrix();
	if ((newdir == dirLEFT) || (newdir == dirRIGHT))
		glRotatef(90.0, 0.0, 1.0, 0.0);
	glCallList(pp->betweenbolts);
    mi->polygon_count += LWO_PipeBetweenBolts.num_pnts/3;
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialGray);
	glCallList(pp->bolts);
    mi->polygon_count += LWO_Bolts3D.num_pnts/3;
	glPopMatrix();

	glCallList(pp->guageconnector);
    mi->polygon_count += LWO_GuageConnector.num_pnts/3;
	glPushMatrix();
	glTranslatef(0.0, 1.33333, 0.0);
	/* Do not change the above to 1 + ONE_THIRD, because */
	/* the object really is centered on 1.3333300000. */
	glRotatef(NRAND(270) + 45.0, 0.0, 0.0, -1.0);
	/* Random rotation for the dial.  I love it. */
	glCallList(pp->guagedial);
    mi->polygon_count += LWO_GuageDial.num_pnts/3;
	glPopMatrix();

	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pp->system_color);
	glCallList(pp->guagehead);
    mi->polygon_count += LWO_GuageHead.num_pnts/3;

	/* GuageFace is drawn last, in case of low-res depth buffers. */
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialWhite);
	glCallList(pp->guageface);
    mi->polygon_count += LWO_GuageFace.num_pnts/3;

	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pp->system_color);
	glFrontFace(GL_CCW);

	return (1);
}


static GLuint
build_teapot(ModeInfo *mi)
{
  pipesstruct *pp = &pipes[MI_SCREEN(mi)];
  GLuint list = glGenLists(1);
  if (!list) return 0;
  glNewList(list, GL_COMPILE);
  pp->teapot_polys = unit_teapot (12, MI_IS_WIREFRAME(mi));
  glEndList();
  return list;
}


static void
MakeTeapot(ModeInfo * mi, int newdir)
{
  pipesstruct *pp = &pipes[MI_SCREEN(mi)];

  switch (newdir) {
  case dirUP:
  case dirDOWN:
    glRotatef(90.0, 1.0, 0.0, 0.0);
    glRotatef(NRAND(3) * 90.0, 0.0, 0.0, 1.0);
    break;
  case dirLEFT:
  case dirRIGHT:
    glRotatef(90.0, 0.0, -1.0, 0.0);
    glRotatef((NRAND(3) * 90.0) - 90.0, 0.0, 0.0, 1.0);
    break;
  case dirNEAR:
  case dirFAR:
    glRotatef(NRAND(4) * 90.0, 0.0, 0.0, 1.0);
    break;
  }

  glCallList(pp->teapot);
  mi->polygon_count += pp->teapot_polys;
  glFrontFace(GL_CCW);
}


static void
MakeShape(ModeInfo * mi, int newdir)
{
  int n = NRAND(100);
  if (n < 50) {
    if (!MakeGuage(mi, newdir))
      MakeTube(mi, newdir);
  } else if (n < 98) {
    MakeValve(mi, newdir);
  } else {
    MakeTeapot(mi,newdir);
  }
}

static void
pinit(ModeInfo * mi, int zera)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];
	int         X, Y, Z;

	if (zera) {
		pp->system_number = 1;
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		(void) memset(pp->Cells, 0, sizeof (pp->Cells));
		for (X = 0; X < HCELLS; X++) {
			for (Y = 0; Y < VCELLS; Y++) {
				pp->Cells[X][Y][0] = 1;
				pp->Cells[X][Y][HCELLS - 1] = 1;
				pp->Cells[0][Y][X] = 1;
				pp->Cells[HCELLS - 1][Y][X] = 1;
			}
		}
		for (X = 0; X < HCELLS; X++) {
			for (Z = 0; Z < HCELLS; Z++) {
				pp->Cells[X][0][Z] = 1;
				pp->Cells[X][VCELLS - 1][Z] = 1;
			}
		}
		(void) memset(pp->usedcolors, 0, sizeof (pp->usedcolors));
	}
	pp->counter = 0;
	pp->turncounter = 0;

	if (!MI_IS_MONO(mi)) {
		int         collist[DEFINEDCOLORS];
		int         i, j, lower = 1000;

		/* Avoid repeating colors on the same screen unless necessary */
		for (i = 0; i < DEFINEDCOLORS; i++) {
			if (lower > pp->usedcolors[i])
				lower = pp->usedcolors[i];
		}
		for (i = 0, j = 0; i < DEFINEDCOLORS; i++) {
			if (pp->usedcolors[i] == lower) {
				collist[j] = i;
				j++;
			}
		}
		i = collist[NRAND(j)];
		pp->usedcolors[i]++;
		switch (i) {
			case 0:
				pp->system_color = MaterialRed;
				break;
			case 1:
				pp->system_color = MaterialGreen;
				break;
			case 2:
				pp->system_color = MaterialBlue;
				break;
			case 3:
				pp->system_color = MaterialCyan;
				break;
			case 4:
				pp->system_color = MaterialYellow;
				break;
			case 5:
				pp->system_color = MaterialMagenta;
				break;
			case 6:
				pp->system_color = MaterialWhite;
				break;
		}
	} else {
		pp->system_color = MaterialGray;
	}

	do {
		pp->PX = NRAND((HCELLS - 1)) + 1;
		pp->PY = NRAND((VCELLS - 1)) + 1;
		pp->PZ = NRAND((HCELLS - 1)) + 1;
	} while (pp->Cells[pp->PX][pp->PY][pp->PZ] ||
		 (pp->Cells[pp->PX + 1][pp->PY][pp->PZ] && pp->Cells[pp->PX - 1][pp->PY][pp->PZ] &&
		  pp->Cells[pp->PX][pp->PY + 1][pp->PZ] && pp->Cells[pp->PX][pp->PY - 1][pp->PZ] &&
		  pp->Cells[pp->PX][pp->PY][pp->PZ + 1] && pp->Cells[pp->PX][pp->PY][pp->PZ - 1]));
	pp->Cells[pp->PX][pp->PY][pp->PZ] = 1;
	pp->olddir = dirNone;

	FindNeighbors(mi);

	pp->nowdir = SelectNeighbor(mi);
}


ENTRYPOINT void
reshape_pipes(ModeInfo * mi, int width, int height)
{
  double h = (GLfloat) height / (GLfloat) width;  
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }
  glViewport(0, y, width, (GLint) height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  /*glFrustum(-1.0, 1.0, -1.0, 1.0, 5.0, 15.0); */
  gluPerspective(65.0, 1/h, 0.1, 20.0);
  glMatrixMode(GL_MODELVIEW);

  glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT Bool
pipes_handle_event (ModeInfo *mi, XEvent *event)
{
  pipesstruct *pp = &pipes[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, pp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &pp->button_down_p))
    return True;
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    {
      pp->fadeout = 100;
      return True;
    }

  return False;
}



static void generate_system (ModeInfo *);


ENTRYPOINT void
init_pipes (ModeInfo * mi)
{
	int         screen = MI_SCREEN(mi);
	pipesstruct *pp;

	MI_INIT (mi, pipes);
	pp = &pipes[screen];

	pp->window = MI_WINDOW(mi);
	if ((pp->glx_context = init_GL(mi)) != NULL) {

		reshape_pipes(mi, MI_WIDTH(mi), MI_HEIGHT(mi));
		if (rotatepipes)
		  pp->initial_rotation = NRAND(180); /* jwz */
		else
		  pp->initial_rotation = -10.0;
		pinit(mi, 1);

		if (factory > 0) {
			pp->valve = BuildLWO(MI_IS_WIREFRAME(mi), &LWO_BigValve);
			pp->bolts = BuildLWO(MI_IS_WIREFRAME(mi), &LWO_Bolts3D);
			pp->betweenbolts = BuildLWO(MI_IS_WIREFRAME(mi), &LWO_PipeBetweenBolts);

			pp->elbowbolts = BuildLWO(MI_IS_WIREFRAME(mi), &LWO_ElbowBolts);
			pp->elbowcoins = BuildLWO(MI_IS_WIREFRAME(mi), &LWO_ElbowCoins);

			pp->guagehead = BuildLWO(MI_IS_WIREFRAME(mi), &LWO_GuageHead);
			pp->guageface = BuildLWO(MI_IS_WIREFRAME(mi), &LWO_GuageFace);
			pp->guagedial = BuildLWO(MI_IS_WIREFRAME(mi), &LWO_GuageDial);
			pp->guageconnector = BuildLWO(MI_IS_WIREFRAME(mi), &LWO_GuageConnector);
			pp->teapot = build_teapot(mi);
		}
		/* else they are all 0, thanks to calloc(). */

		if (MI_COUNT(mi) < 1 || MI_COUNT(mi) > NofSysTypes + 1) {
			pp->system_type = NRAND(NofSysTypes) + 1;
		} else {
			pp->system_type = MI_COUNT(mi);
		}

		if (MI_CYCLES(mi) > 0 && MI_CYCLES(mi) < 11) {
			pp->number_of_systems = MI_CYCLES(mi);
		} else {
			pp->number_of_systems = 5;
		}

		if (MI_SIZE(mi) < 10) {
			pp->system_length = 10;
		} else if (MI_SIZE(mi) > 1000) {
			pp->system_length = 1000;
		} else {
			pp->system_length = MI_SIZE(mi);
		}
	} else {
		MI_CLEARWINDOW(mi);
	}

    pp->trackball = gltrackball_init (True);
    generate_system (mi);
}


static GLuint
get_dlist (ModeInfo *mi, int i)
{
  pipesstruct *pp = &pipes[MI_SCREEN(mi)];
  if (i >= pp->dlist_count)
    {
      pp->dlist_count++;
      if (pp->dlist_count >= pp->dlist_size)
        {
          int s2 = (pp->dlist_size + 100) * 1.2;
          pp->dlists = (GLuint *)
            realloc (pp->dlists, s2 * sizeof(*pp->dlists));
          if (! pp->dlists) abort();
          pp->poly_counts = (GLuint *)
            realloc (pp->poly_counts, s2 * sizeof(*pp->poly_counts));
          if (! pp->poly_counts) abort();
          pp->dlist_size = s2;
        }
      pp->dlists [i] = glGenLists (1);
      pp->poly_counts [i] = 0;
    }
  return pp->dlists[i];
}



static void
generate_system (ModeInfo * mi)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];
    Bool        wire = MI_IS_WIREFRAME(mi);

	int         newdir;
	int         OPX, OPY, OPZ;

    Bool reset_p = False;

    pp->system_index = 0;
    pp->system_size = 0;
    pinit (mi, 1);

    while (1) {
      glNewList (get_dlist (mi, pp->system_size++), GL_COMPILE);
      mi->polygon_count = 0;

	glPushMatrix();

	FindNeighbors(mi);

    if (wire)
      glColor4fv (pp->system_color);
    else
      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pp->system_color);

	/* If it's the begining of a system, draw a sphere */
	if (pp->olddir == dirNone) {
		glPushMatrix();
		glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0);
		mySphere(0.6, wire);
		glPopMatrix();
	}
	/* Check for stop conditions */
	if (pp->ndirections == 0 || pp->counter > pp->system_length) {
		glPushMatrix();
		glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0);
		/* Finish the system with another sphere */
		mySphere(0.6, wire);

		glPopMatrix();

		/* If the maximum number of system was drawn, restart (clearing the screen), */
		/* else start a new system. */
		if (++pp->system_number > pp->number_of_systems) {
          reset_p = True;
		} else {
			pinit(mi, 0);
		}

        goto NEXT;
	}
	pp->counter++;
	pp->turncounter++;

	/* Do will the direction change? if so, determine the new one */
	newdir = pp->nowdir;
	if (!pp->directions[newdir]) {	/* cannot proceed in the current direction */
		newdir = SelectNeighbor(mi);
	} else {
		if (tightturns) {
			/* random change (20% chance) */
			if ((pp->counter > 1) && (NRAND(100) < 20)) {
				newdir = SelectNeighbor(mi);
			}
		} else {
			/* Chance to turn increases after each length of pipe drawn */
			if ((pp->counter > 1) && NRAND(50) < NRAND(pp->turncounter + 1)) {
				newdir = SelectNeighbor(mi);
				pp->turncounter = 0;
			}
		}
	}

	/* Has the direction changed? */
	if (newdir == pp->nowdir) {
		/* If not, draw the cell's center pipe */
		glPushMatrix();
		glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0);
		/* Chance of factory shape here, if enabled. */
		if ((pp->counter > 1) && (NRAND(100) < factory)) {
			MakeShape(mi, newdir);
		} else {
			MakeTube(mi, newdir);
		}
		glPopMatrix();
	} else {
		/* If so, draw the cell's center elbow/sphere */
		int         sysT = pp->system_type;

		if (sysT == NofSysTypes + 1) {
			sysT = ((pp->system_number - 1) % NofSysTypes) + 1;
		}
		glPushMatrix();

		switch (sysT) {
			case 1:
				glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0);
				mySphere(elbowradius, wire);
				break;
			case 2:
			case 3:
				switch (pp->nowdir) {
					case dirUP:
						switch (newdir) {
							case dirLEFT:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 - (one_third), (pp->PY - 12) / 3.0 * 4.0 - (one_third), (pp->PZ - 16) / 3.0 * 4.0);
								glRotatef(180.0, 1.0, 0.0, 0.0);
								break;
							case dirRIGHT:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 + (one_third), (pp->PY - 12) / 3.0 * 4.0 - (one_third), (pp->PZ - 16) / 3.0 * 4.0);
								glRotatef(180.0, 1.0, 0.0, 0.0);
								glRotatef(180.0, 0.0, 1.0, 0.0);
								break;
							case dirFAR:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0 - (one_third), (pp->PZ - 16) / 3.0 * 4.0 - (one_third));
								glRotatef(90.0, 0.0, 1.0, 0.0);
								glRotatef(180.0, 0.0, 0.0, 1.0);
								break;
							case dirNEAR:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0 - (one_third), (pp->PZ - 16) / 3.0 * 4.0 + (one_third));
								glRotatef(90.0, 0.0, 1.0, 0.0);
								glRotatef(180.0, 1.0, 0.0, 0.0);
								break;
						}
						break;
					case dirDOWN:
						switch (newdir) {
							case dirLEFT:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 - (one_third), (pp->PY - 12) / 3.0 * 4.0 + (one_third), (pp->PZ - 16) / 3.0 * 4.0);
								break;
							case dirRIGHT:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 + (one_third), (pp->PY - 12) / 3.0 * 4.0 + (one_third), (pp->PZ - 16) / 3.0 * 4.0);
								glRotatef(180.0, 0.0, 1.0, 0.0);
								break;
							case dirFAR:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0 + (one_third), (pp->PZ - 16) / 3.0 * 4.0 - (one_third));
								glRotatef(270.0, 0.0, 1.0, 0.0);
								break;
							case dirNEAR:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0 + (one_third), (pp->PZ - 16) / 3.0 * 4.0 + (one_third));
								glRotatef(90.0, 0.0, 1.0, 0.0);
								break;
						}
						break;
					case dirLEFT:
						switch (newdir) {
							case dirUP:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 + (one_third), (pp->PY - 12) / 3.0 * 4.0 + (one_third), (pp->PZ - 16) / 3.0 * 4.0);
								glRotatef(180.0, 0.0, 1.0, 0.0);
								break;
							case dirDOWN:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 + (one_third), (pp->PY - 12) / 3.0 * 4.0 - (one_third), (pp->PZ - 16) / 3.0 * 4.0);
								glRotatef(180.0, 1.0, 0.0, 0.0);
								glRotatef(180.0, 0.0, 1.0, 0.0);
								break;
							case dirFAR:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 + (one_third), (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0 - (one_third));
								glRotatef(270.0, 1.0, 0.0, 0.0);
								glRotatef(180.0, 0.0, 1.0, 0.0);
								break;
							case dirNEAR:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 + (one_third), (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0 + (one_third));
								glRotatef(270.0, 1.0, 0.0, 0.0);
								glRotatef(180.0, 0.0, 0.0, 1.0);
								break;
						}
						break;
					case dirRIGHT:
						switch (newdir) {
							case dirUP:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 - (one_third), (pp->PY - 12) / 3.0 * 4.0 + (one_third), (pp->PZ - 16) / 3.0 * 4.0);
								break;
							case dirDOWN:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 - (one_third), (pp->PY - 12) / 3.0 * 4.0 - (one_third), (pp->PZ - 16) / 3.0 * 4.0);
								glRotatef(180.0, 1.0, 0.0, 0.0);
								break;
							case dirFAR:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 - (one_third), (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0 - (one_third));
								glRotatef(270.0, 1.0, 0.0, 0.0);
								break;
							case dirNEAR:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 - (one_third), (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0 + (one_third));
								glRotatef(90.0, 1.0, 0.0, 0.0);
								break;
						}
						break;
					case dirNEAR:
						switch (newdir) {
							case dirLEFT:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 - (one_third), (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0 - (one_third));
								glRotatef(270.0, 1.0, 0.0, 0.0);
								break;
							case dirRIGHT:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 + (one_third), (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0 - (one_third));
								glRotatef(270.0, 1.0, 0.0, 0.0);
								glRotatef(180.0, 0.0, 1.0, 0.0);
								break;
							case dirUP:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0 + (one_third), (pp->PZ - 16) / 3.0 * 4.0 - (one_third));
								glRotatef(270.0, 0.0, 1.0, 0.0);
								break;
							case dirDOWN:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0 - (one_third), (pp->PZ - 16) / 3.0 * 4.0 - (one_third));
								glRotatef(90.0, 0.0, 1.0, 0.0);
								glRotatef(180.0, 0.0, 0.0, 1.0);
								break;
						}
						break;
					case dirFAR:
						switch (newdir) {
							case dirUP:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0 + (one_third), (pp->PZ - 16) / 3.0 * 4.0 + (one_third));
								glRotatef(90.0, 0.0, 1.0, 0.0);
								break;
							case dirDOWN:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0, (pp->PY - 12) / 3.0 * 4.0 - (one_third), (pp->PZ - 16) / 3.0 * 4.0 + (one_third));
								glRotatef(90.0, 0.0, 1.0, 0.0);
								glRotatef(180.0, 1.0, 0.0, 0.0);
								break;
							case dirLEFT:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 - (one_third), (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0 + (one_third));
								glRotatef(90.0, 1.0, 0.0, 0.0);
								break;
							case dirRIGHT:
								glTranslatef((pp->PX - 16) / 3.0 * 4.0 + (one_third), (pp->PY - 12) / 3.0 * 4.0, (pp->PZ - 16) / 3.0 * 4.0 + (one_third));
								glRotatef(270.0, 1.0, 0.0, 0.0);
								glRotatef(180.0, 0.0, 0.0, 1.0);
								break;
						}
						break;
				}
				myElbow(mi, (sysT == 2));
				break;
		}
		glPopMatrix();
	}

	OPX = pp->PX;
	OPY = pp->PY;
	OPZ = pp->PZ;
	pp->olddir = pp->nowdir;
	pp->nowdir = newdir;
	switch (pp->nowdir) {
		case dirUP:
			pp->PY++;
			break;
		case dirDOWN:
			pp->PY--;
			break;
		case dirLEFT:
			pp->PX--;
			break;
		case dirRIGHT:
			pp->PX++;
			break;
		case dirNEAR:
			pp->PZ++;
			break;
		case dirFAR:
			pp->PZ--;
			break;
	}
	pp->Cells[pp->PX][pp->PY][pp->PZ] = 1;

	/* Cells'face pipe */
	glTranslatef(((pp->PX + OPX) / 2.0 - 16) / 3.0 * 4.0, ((pp->PY + OPY) / 2.0 - 12) / 3.0 * 4.0, ((pp->PZ + OPZ) / 2.0 - 16) / 3.0 * 4.0);
	MakeTube(mi, newdir);

    NEXT:
	glPopMatrix();
    glEndList();
    pp->poly_counts [pp->system_size-1] = mi->polygon_count;

    if (reset_p)
      break;
    }
}


ENTRYPOINT void
draw_pipes (ModeInfo * mi)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];
	Display *display = MI_DISPLAY(mi);
	Window    window = MI_WINDOW(mi);
    Bool        wire = MI_IS_WIREFRAME(mi);
    int i = 0;

	if (!pp->glx_context)
		return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *pp->glx_context);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glColor3f(1.0, 1.0, 1.0);

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse0);
	glLightfv(GL_LIGHT0, GL_POSITION, position0);
	glLightfv(GL_LIGHT1, GL_AMBIENT, ambient1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse1);
	glLightfv(GL_LIGHT1, GL_POSITION, position1);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
	glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);

    if (wire)
      glDisable(GL_LIGHTING);
    else
      {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        /* This looks crappy. */
        /* glEnable(GL_LIGHT1); */
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_NORMALIZE);
        glEnable(GL_CULL_FACE);
      }

	glShadeModel(GL_SMOOTH);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, front_shininess);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, front_specular);

    glPushMatrix();

    pp->initial_rotation += 0.02;

	glTranslatef(0.0, 0.0, fisheye ? -3.8 : -4.8);

    gltrackball_rotate (pp->trackball);

	if (rotatepipes)
      glRotatef(pp->initial_rotation, 0.0, 1.0, 0.0);

    glScalef(Scale4Window, Scale4Window, Scale4Window);

    mi->polygon_count = 0;

    if (pp->fadeout)
      {
        GLfloat s = (pp->fadeout * pp->fadeout) / 10000.0;
        glScalef (s, s, s);
        glRotatef (90 * (1 - (pp->fadeout/100.0)), 1, 0, 0.1);
        pp->fadeout -= 4;
        if (pp->fadeout <= 0)
          {
            pp->fadeout = 0;
            generate_system (mi);
          }
      }
    else if (pp->system_index < pp->system_size)
      pp->system_index++;
    else
      pp->fadeout = 100;

    for (i = 0; i < pp->system_index; i++)
      {
        glCallList (pp->dlists[i]);
        mi->polygon_count += pp->poly_counts[i];
      }

    glPopMatrix();

    if (mi->fps_p) do_fps (mi);
    glFinish();

    glXSwapBuffers(display, window);
}


#ifndef STANDALONE
ENTRYPOINT void
change_pipes (ModeInfo * mi)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];

	if (!pp->glx_context)
		return;

	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *pp->glx_context);
	pinit(mi, 1);
}
#endif /* !STANDALONE */


ENTRYPOINT void
free_pipes (ModeInfo * mi)
{
	pipesstruct *pp = &pipes[MI_SCREEN(mi)];

    if (!pp->glx_context) return;
	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *pp->glx_context);

    if (pp->trackball) gltrackball_free (pp->trackball);

    if (pp->valve)
        glDeleteLists(pp->valve, 1);
    if (pp->bolts)
        glDeleteLists(pp->bolts, 1);
    if (pp->betweenbolts)
        glDeleteLists(pp->betweenbolts, 1);

    if (pp->elbowbolts)
        glDeleteLists(pp->elbowbolts, 1);
    if (pp->elbowcoins)
        glDeleteLists(pp->elbowcoins, 1);

    if (pp->guagehead)
        glDeleteLists(pp->guagehead, 1);
    if (pp->guageface)
        glDeleteLists(pp->guageface, 1);
    if (pp->guagedial)
        glDeleteLists(pp->guagedial, 1);
    if (pp->guageconnector)
        glDeleteLists(pp->guageconnector, 1);
    if (pp->teapot)
        glDeleteLists(pp->teapot, 1);
    if (pp->dlists)
      {
        int i;
        for (i = 0; i < pp->dlist_count; i++)
          glDeleteLists (pp->dlists[i], 1);
        free (pp->dlists);
        free (pp->poly_counts);
      }
}

XSCREENSAVER_MODULE ("Pipes", pipes)

#endif
