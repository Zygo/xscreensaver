/* -*- Mode: C; tab-width: 4 -*- */
/* life3d --- Extension to Conway's game of Life, Carter Bays' 4555 3d life */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)life3d.c	4.04 97/07/28 xlockmore";

#endif

/*-
 * Copyright (c) 1994 by David Bagley.
 *
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
 * Revision History:
 * 10-May-97: Compatible with xscreensaver
 * 18-Apr-97: Memory leak fixed by Tom Schmidt <tschmidt@micron.com>
 * 12-Mar-95: added LIFE_5766 compile-time option
 * 12-Feb-95: shooting gliders added
 * 07-Dec-94: used life.c and a DOS version of 3dlife
 * Copyright 1993 Anthony Wesley awesley@canb.auug.org.au found at
 * life.anu.edu.au /pub/complex_systems/alife/3DLIFE.ZIP
 * There is some flashing that was not in the original.  This is because 
 * the direct video memory access garbage collection was not portable.
 *
 *
 * References:
 * Dewdney, A.K., "The Armchair Universe, Computer Recreations from the
 *   Pages of Scientific American Magazine", W.H. Freedman and Company,
 *   New York, 1988 (February 1987 p 16)
 * Bays, Carter, "The Game of Three Dimensional Life", 86/11/20
 *   with (latest?) update from 87/2/1
 */

#ifdef STANDALONE
#define PROGCLASS "Life3D"
#define HACK_INIT init_life3d
#define HACK_DRAW draw_life3d
#define life3d_opts xlockmore_opts
#define DEFAULTS "*delay: 1000000 \n" \
 "*count: 35 \n" \
 "*cycles: 85 \n" \
 "*ncolors: 200 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

/* #define DEF_RULE3D  "0" */
#define DEF_RULE3D  "1"
/* #define DEF_RULE3D  "4555" */
/* #define DEF_RULE3D  "5766" */

static int  rule3d;

static XrmOptionDescRec opts[] =
{
	{"-rule3d", ".life3d.rule3d", XrmoptionSepArg, (caddr_t) NULL},
};
static argtype vars[] =
{
	{(caddr_t *) & rule3d, "rule3d", "Rule3D", DEF_RULE3D, t_Int}
};
static OptionStruct desc[] =
{
 {"-rule3d num", "<living_min><living_max><birth_min><birth_max> parameters"}
};

ModeSpecOpt life3d_opts =
{1, opts, 1, vars, desc};

#define ON 0x40
#define OFF 0

/* Don't change these numbers without changing the offset() macro below! */
#define MAXSTACKS 64
#define	MAXROWS 128
#define MAXCOLUMNS 128
#define BASESIZE ((MAXCOLUMNS*MAXROWS*MAXSTACKS)>>6)

#define RT_ANGLE 90
#define HALFRT_ANGLE 45
/* Store state of cell in top bit. Reserve low bits for count of living nbrs */
#define Set3D(x,y,z) SetMem(lp,(unsigned int)x,(unsigned int)y,(unsigned int)z,ON)
#define Reset3D(x,y,z) SetMem(lp,(unsigned int)x,(unsigned int)y,(unsigned int)z,OFF)

#define SetList3D(x,y,z) SetMem(lp,(unsigned int)x,(unsigned int)y,(unsigned int)z,ON), AddToList(lp,(unsigned int)x,(unsigned int)y,(unsigned int)z)

#define CellState3D(c) ((c)&ON)
#define CellNbrs3D(c) ((c)&0x1f)	/* 26 <= 31 */

#define EyeToScreen 72.0	/* distance from eye to screen */
#define HalfScreenD 14.0	/* 1/2 the diameter of screen */
#define BUCKETSIZE 10
#define NBUCKETS ((MAXCOLUMNS+MAXROWS+MAXSTACKS)*BUCKETSIZE)
#define Distance(x1,y1,z1,x2,y2,z2) sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2)+(z1-z2)*(z1-z2))

#define IP 0.0174533

#define COLORBASE 3
#define COLORS (COLORBASE + 2)
#define COLORSTEP (NUMCOLORS/COLORBASE)		/* 21 different colors per side */

#define BLACK 0
#define RED 1
#define GREEN 2
#define BLUE 3
#define WHITE 4
#define NUMPTS 42

typedef struct _CellList {
	unsigned char x, y, z;	/* Location in world coordinates */
	char        visible;	/* 1 if the cube is to be displayed */
	short       dist;	/* dist from cell to eye */
	struct _CellList *next;	/* pointer to next entry on linked list */
	struct _CellList *prev;
	struct _CellList *priority;
} CellList;

typedef struct {
	int         living_min, living_max, birth_min, birth_max;
	int         pattern, patterned_rule;
	int         rule3d;
	int         initialized;
	int         wireframe;	/* FALSE */
	int         ox, oy, oz;	/* origin */
	double      vx, vy, vz;	/* viewpoint */
	int         generation;
	int         nstacks, nrows, ncolumns;
	int         memstart;
	char        visible;
	unsigned char *base[BASESIZE];
	double      A, B, C, F;
	int         width, height;
	unsigned long color[COLORS];
	int         alt, azm;
	double      dist;
	CellList   *ptrhead, *ptrend, eraserhead, eraserend;
	CellList   *buckethead[NBUCKETS], *bucketend[NBUCKETS];		/* comfortable upper b */
} life3dstruct;


static life3dstruct *life3ds = NULL;

static int  DrawCube(ModeInfo * mi, CellList * cell);

/* 4555 life is probably the best */
/* 5766 life has gliders like 2d 2333 life */
/* There are no known gliders for 5655 or 6767 so the others may be better */
#define LIFE_4555 0
#define LIFE_5766 1
#define LIFE_GLIDERS 2
#define LIFE_5655 2
#define LIFE_6767 3
#define LIFE_RULES 4

static char patterns_4555[][3 * NUMPTS + 1] =
{
#if 0
/* Still life */
	{			/* V */
		-1, -1, -1, 0, -1, -1,
		-1, 0, -1, 0, 0, -1,

		-1, -1, 0, 0, -1, 0,
		127
	},
	{			/* CROSS */
		0, 0, -1,

		0, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,

		0, 0, 1,
		127
	},
	{			/* PILLAR */
		0, -1, -1,
		-1, 0, -1, 1, 0, -1,
		0, 1, -1,

		0, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,
		127
	},
#endif
	{			/* BLINKER P2 */
		0, 0, -1,

		0, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,

		-1, 0, 1, 1, 0, 1,
		127
	},
	{			/* DOUBLE BLINKER P2 */
		0, -1, -1,
		0, 1, -1,

		0, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,

		-1, 0, 1, 1, 0, 1,
		127
	},
	{			/* TRIPLE BLINKER 1 P2 */
		-1, -1, -2,
		-1, 0, -2,

		-2, -1, -1,
		1, 0, -1,
		-1, 1, -1, 0, 1, -1,

		-1, -2, 0, 0, -2, 0,
		-2, -1, 0, 1, 0, 0,

		0, -1, 1,
		0, 0, 1,
		127
	},
	{			/* TRIPLE BLINKER 2 P2 */
		-1, -1, -2, 0, -1, -2,

		0, -2, -1,
		1, -1, -1,
		1, 0, -1,
		-1, 1, -1,

		0, -2, 0,
		-2, -1, 0,
		-2, 0, 0,
		-1, 1, 0,

		-1, 0, 1, 0, 0, 1,
		127
	},
	{			/* THREE HALFS BLINKER  P2 */
		0, -1, -1,
		-1, 0, -1, 1, 0, -1,

		-1, -1, 0,
		1, 0, 0,
		-1, 1, 0, 0, 1, 0,

		0, -1, 1,
		0, 0, 1,
		127
	},
	{			/* PUFFER P4 */
		0, -1, -1,
		0, 0, -1,

		0, -2, 0,
		-1, -1, 0, 1, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,
		127
	},
	{			/* PINWHEEL P4 */
		-1, 1, -1, 0, 1, -1,

		-1, -1, 0, 0, -1, 0,
		-2, 0, 0, 1, 0, 0,

		-1, 0, 1, 0, 0, 1,
		-1, 1, 1, 0, 1, 1,
		127
	},
	{			/* HEART P4 */
		-1, -1, -1,
		-1, 0, -1, 0, 0, -1,

		0, -1, 0,
		-2, 0, 0, 1, 0, 0,

		-1, -1, 1,
		-1, 1, 1, 0, 1, 1,
		127
	},
	{			/* ARROW P4 */
		0, -1, -1,
		0, 0, -1,

		0, -2, 0,
		1, -1, 0,
		1, 0, 0,
		0, 1, 0,

		-1, -1, 1,
		-1, 0, 1,
		127
	},
	{			/* ROTOR P2 */
		0, -1, -1,
		0, 0, -1,

		0, -2, 0,
		-1, -1, 0, 1, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,

		0, -1, 1,
		0, 0, 1,
		127
	},
	{			/* BRONCO P4 */
		0, -1, -1,
		0, 0, -1,

		0, -2, 0,
		-1, -1, 0, 1, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,

		-1, -1, 1,
		-1, 0, 1,
		127
	},
	{			/* TRIPUMP P2 */
		0, -2, -2,
		-2, -1, -2, -1, -1, -2, 0, -1, -2,

		0, -2, -1,
		-2, 0, -1,
		-2, 1, -1, -1, 1, -1,

		1, -2, 0,
		1, -1, 0,
		1, 0, 0,
		-1, 1, 0,

		0, 0, 1, 1, 0, 1,
		-1, 1, 1,
		127
	},
	{			/* WINDSHIELDWIPER (HELICOPTER) P2 */
		-2, -1, -2, -1, -1, -2,
		0, 0, -2,

		-1, -2, -1,
		-2, -1, -1,
		1, 0, -1,
		-1, 1, -1, 0, 1, -1, 1, 1, -1,

		0, -2, 0,
		-2, -1, 0, 1, -1, 0,

		0, -2, 1,
		0, -1, 1,
		0, 0, 1,
		127
	},
	{			/* WALTZER P6 */
		-2, -1, -1, -1, -1, -1, 0, -1, -1,
		-2, 0, -1,
		-1, 1, -1, 0, 1, -1,

		-2, -1, 0,
		-2, 0, 0, 1, 0, 0,
		1, 1, 0,

		-1, 0, 1, 0, 0, 1,
		0, 1, 1,
		127
	},
	{			/* BIG WALTZER P6 */
		0, -1, -1, 1, -1, -1,
		-1, 0, -1, 0, 0, -1,
		-1, 1, -1,

		0, -2, 0, 1, -2, 0,
		-2, 0, 0,
		-2, 1, 0,

		0, -1, 1, 1, -1, 1,
		-1, 0, 1, 0, 0, 1,
		-1, 1, 1,
		127
	},
	{			/* SEESAW P2 */
		0, 0, -2,

		-2, -1, -1, -1, -1, -1,
		-2, 0, -1,
		0, 1, -1,

		-1, -1, 0,
		1, 0, 0,
		0, 1, 0, 1, 1, 0,

		-1, 0, 1,
		127
	},
	{			/* COLLISION -> REDIRECTION */
		-3, 5, -7, 0, 5, -7,
		-3, 4, -7, 0, 4, -7,
		-2, 3, -7, -1, 3, -7,

		-2, 5, -6, -1, 5, -6,
		-2, 4, -6, -1, 4, -6,

		1, -2, 1,

		1, -3, 2,
		0, -2, 2, 2, -2, 2,
		1, -1, 2,

		0, -2, 3, 2, -2, 3,
		127
	},
	{			/* COLLISION -> SEESAW P2 */
		-4, -6, -7, -1, -6, -7,
		-4, -5, -7, -1, -5, -7,
		-3, -4, -7, -2, -4, -7,

		-3, -6, -6, -2, -6, -6,
		-3, -5, -6, -2, -5, -6,

		1, 4, 6, 2, 4, 6,
		0, 5, 6, 3, 5, 6,
		0, 6, 6, 3, 6, 6,

		1, 5, 5, 2, 5, 5,
		1, 6, 5, 2, 6, 5,
		127
	}
};

static char patterns_5766[][3 * NUMPTS + 1] =
{
	{			/* KNIFE-SWITCH BLINKER */
		0, -1, -1,
		-1, 0, -1, 0, 0, -1, 1, 0, -1,

		0, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 0, 1,
		127,
	},
	{			/* CLOCK */
		0, -1, -2,
		0, 0, -2,

		-2, -1, -1, 0, -1, -1,
		-2, 0, -1, 0, 0, -1,

		-1, -1, 0, 1, -1, 0,
		-1, 0, 0, 1, 0, 0,

		-1, -1, 1,
		-1, 0, 1,
		127,
	},
	{			/* HALF BLINKERS */
		0, -1, -2,
		0, 0, -2,

		-1, -1, -1, 1, -1, -1,
		-1, 0, -1, 1, 0, -1,
		0, 1, -1,

		-1, -2, 0, 0, -2, 0, 1, -2, 0,
		-2, -1, 0, 2, -1, 0,
		-2, 0, 0, 2, 0, 0,
		0, 1, 0,

		-1, -1, 1, 1, -1, 1,
		-1, 0, 1, 1, 0, 1,
		0, 1, 1,

		0, -1, 2,
		0, 0, 2,
		127,
	},
	{			/* MUTANT HALF BLINKERS */
		0, -1, -2,
		0, 0, -2,

		0, -2, -1,
		-1, -1, -1, 1, -1, -1,
		-1, 0, -1, 1, 0, -1,
		0, 1, -1,

		-1, -2, 0, 1, -2, 0,
		-2, -1, 0, 2, -1, 0,
		-2, 0, 0, 2, 0, 0,
		0, 1, 0,

		0, -2, 1, 1, -2, 1,
		-1, -1, 1, 2, -1, 1,
		-1, 0, 1, 2, 0, 1,
		0, 1, 1,

		0, -1, 2, 1, -1, 2,
		0, 0, 2,
		127,
	},
	{			/* FUSE */
		0, 0, 4,

		0, 0, 3,

		0, -2, 2,
		-1, -1, 2, 1, -1, 2,
		-2, 0, 2, 2, 0, 2,
		-1, 1, 2, 1, 1, 2,
		0, 2, 2,

		0, -2, 1,
		-1, -1, 1, 1, -1, 1,
		-2, 0, 1, 2, 0, 1,
		-1, 1, 1, 1, 1, 1,
		0, 2, 1,

		0, 0, 0,

		0, 0, -1,

		0, -2, -2,
		-1, -1, -2, 1, -1, -2,
		-2, 0, -2, 2, 0, -2,
		-1, 1, -2, 1, 1, -2,
		0, 2, -2,

		0, -2, -3,
		-1, -1, -3, 1, -1, -3,
		-2, 0, -3, 2, 0, -3,
		-1, 1, -3, 1, 1, -3,
		0, 2, -3,

		-1, 0, -4, 0, 0, -4, 1, 0, -4,
		127,
	},
	{			/* 2 PTS OF STAR OF DAVID */
		1, 0, -3, 2, 0, -3,

		1, -1, -2, 2, -1, -2,
		0, 0, -2,
		1, 1, -2, 2, 1, -2,

		1, 0, -1, 2, 0, -1,

		-2, 0, 0, 0, 0, 0,

		-2, -1, 1,
		-3, 0, 1, -1, 0, 1,
		-2, 1, 1,

		-2, -1, 2,
		-3, 0, 2, -1, 0, 2,
		-2, 1, 2,
		127,
	},
	{			/* TRIPLE BLINKER 1 */
		0, -1, -2,
		0, 0, -2,

		-1, -1, -1, 1, -1, -1,
		-2, 0, -1,
		-1, 1, -1, 0, 1, -1,

		-1, -2, 0, 0, -2, 0,
		1, -1, 0,
		-2, 0, 0, 0, 0, 0,

		-1, -1, 1,
		-1, 0, 1,
		127,
	},
	{			/* TRIPLE BLINKER 2 */
		-1, -1, -2, 0, -1, -2,

		-1, -2, -1,
		-2, -1, -1, -1, -1, -1,
		-2, 0, -1,
		0, 1, -1,

		-1, -2, 0,
		1, -1, 0,
		0, 0, 0, 1, 0, 0,
		0, 1, 0,

		-1, 0, 1, 0, 0, 1,
		127,
	},
	{			/* TRIKNOT */
		0, 0, -1, 1, 0, -1,

		-1, -1, 0, 0, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,

		0, 0, 1,
		0, 1, 1,
		127,
	},
	{			/* SEARCHLIGHT */
		-1, -1, -2, 0, -1, -2,
		-1, 0, -2, 0, 0, -2,

		-1, -2, -1, 0, -2, -1,
		-2, -1, -1, 1, -1, -1,
		-2, 0, -1, 1, 0, -1,
		0, 1, -1,

		-1, -2, 0, 0, -2, 0,
		1, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,

		-1, -1, 1, 0, -1, 1,
		127,
	},
	{			/* POLE DRIVER */
		-1, -1, -1, 0, -1, -1,
		-1, 0, -1, 1, 0, -1,
		0, 1, -1, 1, 1, -1,

		0, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0,

		0, 0, 1,
		127,
	},
	{			/* LITTLE STAR */
		0, -1, -1,
		-1, 0, -1, 1, 0, -1,
		-1, 1, -1, 0, 1, -1, 1, 1, -1,

		-1, -1, 0, 1, -1, 0,
		-1, 1, 0, 1, 1, 0,

		-1, -1, 1, 0, -1, 1, 1, -1, 1,
		-1, 0, 1, 1, 0, 1,
		0, 1, 1,
		127,
	},
	{			/* JAWS */
		0, -1, -2,
		-1, 0, -2, 1, 0, -2,
		0, 1, -2,

		0, -2, -1,
		-1, -1, -1, 1, -1, -1,
		-1, 0, -1, 2, 0, -1,
		0, 1, -1, 1, 1, -1,

		-1, -2, 0, 1, -2, 0,
		-1, -1, 0, 2, -1, 0,
		1, 1, 0, 2, 1, 0,

		0, -2, 1,
		0, -1, 1, 1, -1, 1,
		1, 0, 1, 2, 0, 1,

		-2, 1, 1,
		-2, 2, 1, -1, 2, 1,

		-2, 1, 2, -1, 1, 2,
		-2, 2, 2, -1, 2, 2,
		127,
	},
	{			/* NEAR SHIP */
		-2, -1, -2, -1, -1, -2,
		-2, 0, -2, -1, 0, -2,

		-2, -1, -1, -1, -1, -1,
		-2, 0, -1, -1, 0, -1,

		0, -1, 0,
		0, 0, 0,

		-2, -1, 1, -1, -1, 1,
		-2, 0, 1, -1, 0, 1,

		-2, -1, 2, -1, -1, 2,
		-2, 0, 2, -1, 0, 2,
		127
	}
};

/* Many names of 5655 & 6767 are made up by bagleyd They are listed in order
   given by Carter Bays all p2 except "H"  */
static char patterns_5655[][3 * NUMPTS + 1] =
{
	{			/* SEESAW */
		0, -1, -1,
		-1, 0, -1, 0, 0, -1,

		0, 0, 0, 1, 0, 0,
		0, 1, 0,
		127
	},
	{			/* PROP */
		-1, 0, -1, 0, 0, -1, 1, 0, -1,

		0, -1, 0,
		0, 0, 0,
		0, 1, 0,
		127
	},
	{			/*  */
		-1, 0, -1, 0, 0, -1,

		0, -1, 0,
		0, 0, 0, 1, 0, 0,
		0, 1, 0,
		127
	},
	{			/* COLUMN */
		0, 0, -6,

		0, 0, -5,

		0, -1, -4,
		-1, 0, -4, 0, 0, -4, 1, 0, -4,
		0, 1, -4,

		0, -1, -1,
		-1, 0, -1, 0, 0, -1, 1, 0, -1,
		0, 1, -1,

		0, 0, 0,

		0, 0, 1,

		0, 0, 2,

		0, 0, 3,

		0, -1, 4,
		-1, 0, 4, 0, 0, 4, 1, 0, 4,
		0, 1, 4,
		127
	},
	{			/* FLIPPING C */
		-1, 0, -1, 1, 0, -1,

		-1, 0, 0, 0, 0, 0, 1, 0, 0,
		127
	},
	{			/* SLIDING BLOCKS  */
		-1, 0, -1, 0, 0, -1,
		-1, 1, -1, 0, 1, -1,

		-1, -1, 0, 0, -1, 0,
		0, 0, 0,
		127
	},
	{			/*  */
		-1, -1, -1,
		-1, 1, -1,

		-1, 0, 0, 0, 0, 0, 1, 0, 0,

		-1, -1, 1,
		-1, 1, 1,
		127
	},
	{			/* Y */
		1, -1, -1,
		-1, 0, -1,
		0, 1, -1,

		1, -1, 0,
		-1, 0, 0, 0, 0, 0,
		0, 1, 0,
		127
	},
	{			/* PUMP */
		0, -1, -1,
		-1, 0, -1, 1, 0, -1,
		0, 1, -1,

		-1, -1, 0, 0, -1, 0,
		-1, 0, 0, 1, 0, 0,
		0, 1, 0, 1, 1, 0,
		127
	},
	{			/* WALKER */
		-1, -1, -1, 0, -1, -1,
		1, 0, -1,

		0, -1, 0, 1, -1, 0,
		-1, 0, 0, 0, 0, 0,
		127
	},
	{			/* SWITCH */
		2, -1, -2,
		1, 0, -2, 2, 0, -2,

		1, -1, -1, 2, -1, -1,
		1, 0, -1,

		1, 0, 0,

		-1, -1, 1,
		-2, 0, 1, -1, 0, 1, 0, 0, 1,

		-2, -1, 2, -1, -1, 2,
		-2, 0, 2,
		127
	},
	{			/* HOPPER */
		-1, -1, -1, 0, -1, -1, 1, -1, -1,

		0, -1, 0,
		-1, 0, 0, 0, 0, 0, 1, 0, 0,
		127
	},
	{			/* PUSHUPS */
		-1, -2, -2,

		-1, -2, -1, 0, -2, -1,
		-1, -1, -1, 0, -1, -1,

		-1, 0, 0, 0, 0, 0,

		-1, 0, 1, 0, 0, 1,
		-1, 1, 1,
		127
	},
	{			/* PUSHER */
		0, -2, -2,
		0, -1, -2,

		-1, -2, -1,
		-1, -1, -1, 0, -1, -1,

		-1, 0, 0, 0, 0, 0,
		0, 1, 0,

		-1, 0, 1,
		0, 1, 1,
		127
	},
	{			/* CAPACITOR */
		0, -1, -2,
		-1, 0, -2, 0, 0, -2,

		0, 0, -1, 1, 0, -1,
		0, 1, -1, 1, 1, -1,

		0, 0, 1, 1, 0, 1,
		0, 1, 1, 1, 1, 1,

		0, -1, 2,
		-1, 0, 2, 0, 0, 2,
		127
	},
	{			/* CORNER 1 */
		-1, -3, -3,
		-1, -2, -3, 0, -2, -3,
		1, -1, -3,
		1, 0, -3, 2, 0, -3,

		-1, -3, -2,
		0, -2, -2,
		1, -1, -2,
		2, 0, -2,

		-3, -3, -1, -2, -3, -1,
		-3, -2, -1,
		2, 1, -1,
		1, 2, -1, 2, 2, -1,

		-3, -2, 0, -2, -2, 0,
		1, 1, 0,
		1, 2, 0,

		-3, -1, 1, -2, -1, 1,
		-3, 0, 1,
		0, 1, 1,
		-1, 2, 1, 0, 2, 1,

		-3, 0, 2, -2, 0, 2,
		-1, 1, 2,
		-1, 2, 2,
		127
	},
	{			/* CORNER 2 */
		-2, -2, -3, -1, -2, -3,
		-2, -1, -3,
		2, 1, -3,
		1, 2, -3, 2, 2, -3,

		-3, -2, -2, -1, -2, -2,
		-3, -1, -2, 0, -1, -2,
		1, 0, -2,
		2, 1, -2,
		1, 3, -2, 2, 3, -2,

		-3, -2, -1, -2, -2, -1,
		0, -1, -1,
		1, 0, -1,
		2, 2, -1,
		2, 3, -1,

		-2, -1, 0, -1, -1, 0,
		1, 1, 0,
		1, 2, 0,

		-2, 0, 1, -1, 0, 1,
		0, 1, 1,
		-3, 2, 1, 0, 2, 1,
		-2, 3, 1,

		-3, 1, 2, -2, 1, 2,
		-3, 2, 2, -1, 2, 2,
		-2, 3, 2, -1, 3, 2,
		127
	},
	{			/* RUNNER */
		-1, -2, -1, 0, -2, -1,
		0, -1, -1,

		-1, -1, 0, 0, -1, 0,

		-1, 0, 1, 0, 0, 1,
		0, 1, 1,

		-1, 1, 2, 0, 1, 2,
		127
	},
	{			/* BACKWARDS RUNNER */
		-1, -2, -1, 0, -2, -1,
		0, -1, -1,

		-1, -1, 0, 0, -1, 0,

		-1, 0, 1, 0, 0, 1,
		-1, 1, 1,

		-1, 0, 2, 0, 0, 2,
		127
	},
	{			/* FLIPPING H */
		-1, 0, -1, 1, 0, -1,

		-1, 0, 0, 0, 0, 0, 1, 0, 0,

		-1, 0, 1, 1, 0, 1,
		127
	},
	{			/* EAGLE */
		0, -2, -1, 1, -2, -1,
		0, -1, -1, 1, -1, -1,
		0, 1, -1, 1, 1, -1,
		0, 2, -1, 1, 2, -1,

		0, -1, 0,
		-1, 0, 0,
		0, 1, 0,
		127
	},
	{			/* VARIANT 1 */
		1, -1, -2,
		0, 0, -2,

		0, -1, -1,
		0, 0, -1, 1, 0, -1,

		-1, -1, 0,

		0, -1, 1,
		0, 0, 1, 1, 0, 1,

		1, -1, 2,
		0, 0, 2,
		127
	},
	{			/* VARIANT 2 */
		0, -1, -2,
		1, 0, -2,

		0, -1, -1, 1, -1, -1,
		0, 0, -1,

		-1, 0, 0,

		0, -1, 1,
		0, 0, 1, 1, 0, 1,

		1, -1, 2,
		0, 0, 2,
		127
	},
	{			/* VARIANT 3 */
		1, -1, -2,
		0, 0, -2,

		0, -1, -1, 1, -1, -1,
		0, 0, -1,

		-1, -1, 0,

		0, -1, 1,
		0, 0, 1, 1, 0, 1,

		1, -1, 2,
		0, 0, 2,
		127
	},
	{			/* VARIANT 4 */
		1, -1, -2,
		0, 0, -2,

		0, -1, -1, 1, -1, -1,
		0, 0, -1,

		-1, 0, 0,

		0, -1, 1,
		0, 0, 1, 1, 0, 1,

		1, -1, 2,
		0, 0, 2,
		127
	},
	{			/* SQUID */
		-2, -1, -2, -1, -1, -2,
		1, 1, -2,
		1, 2, -2,

		-2, -1, -1, -1, -1, -1, 0, -1, -1,
		1, 0, -1,
		1, 1, -1,
		1, 2, -1,

		-1, -1, 0,
		1, 1, 0,

		-1, 0, 1,
		-2, 1, 1, -1, 1, 1, 0, 1, 1,
		-2, 2, 1, -1, 2, 1,
		127
	},
	{			/* ROWER */
		0, 1, -2,

		-1, -1, -1, 1, -1, -1,
		-2, 0, -1, -1, 0, -1, 1, 0, -1, 2, 0, -1,

		-1, 0, 0, 1, 0, 0,
		-1, 1, 0, 1, 1, 0,

		0, -1, 1,
		127
	},
	{			/* FLIP */
		-1, -2, -2,
		0, -1, -2,
		0, 0, -2,

		-1, -2, -1,
		-2, -1, -1, 1, -1, -1,
		1, 0, -1,
		1, 1, -1,

		-1, -2, 0,
		-2, -1, 0,
		-1, 1, 0, 0, 1, 0,

		-2, 0, 1, -1, 0, 1, 0, 0, 1,
		127
	}
};
static char patterns_6767[][3 * NUMPTS + 1] =
{
	{			/* WALKING BOX */
		-1, -1, -2,
		0, 0, -2,

		-1, -2, -1, 0, -2, -1,
		-2, -1, -1, 1, -1, -1,
		-2, 0, -1, 1, 0, -1,
		-1, 1, -1, 0, 1, -1,

		-1, -2, 0, 0, -2, 0,
		-2, -1, 0, 1, -1, 0,
		-2, 0, 0, 1, 0, 0,
		-1, 1, 0, 0, 1, 0,

		-1, -1, 1,
		0, 0, 1,
		127
	},
	{			/* WALKER */
		0, -1, -1,
		-1, 0, -1,

		-1, -1, 0, 0, -1, 0,
		-1, 0, 0, 0, 0, 0,

		0, -1, 1,
		-1, 0, 1,
		127
	},
	{			/* S */
		-1, -1, -1, 0, -1, -1,
		-1, 0, -1, 0, 0, -1,

		-1, 0, 0, 0, 0, 0,
		-1, 1, 0, 0, 1, 0,
		127
	},
	{			/* BACKWARDS ARROW */
		-1, 0, -1, 0, 0, -1,

		-1, 0, 0, 0, 0, 0,
		-1, 1, 0, 0, 1, 0,

		-1, -1, 1, 0, -1, 1,
		127
	},
	{			/* SPINING BOX */
		-1, -1, -2, 0, -1, -2,
		-1, 0, -2, 0, 0, -2,

		0, -2, -1,
		-2, -1, -1, 1, -1, -1,
		-2, 0, -1,
		-1, 1, -1, 0, 1, -1,

		-1, -2, 0, 0, -2, 0,
		-2, -1, 0, 1, -1, 0,
		-2, 0, 0, 1, 0, 0,
		-1, 1, 0, 0, 1, 0,

		-1, -1, 1, 0, -1, 1,
		0, 0, 1,
		127
	},
	{			/* FLIPPING T */
		0, 0, -1,

		0, 0, 0,

		0, -1, 1,
		-1, 0, 1, 0, 0, 1, 1, 0, 1,
		-1, 1, 1, 0, 1, 1,
		127
	}
};

static int  npatterns[] =
{
	(sizeof patterns_4555 / sizeof patterns_4555[0]),
	(sizeof patterns_5766 / sizeof patterns_5766[0]),
	(sizeof patterns_5655 / sizeof patterns_5655[0]),
	(sizeof patterns_6767 / sizeof patterns_6767[0]),
};

static int
Rule2Array(int rule)
{
	switch (rule) {
		case 4555:
			return LIFE_4555;
		case 5766:
			return LIFE_5766;
		case 5655:
			return LIFE_5655;
		case 6767:
			return LIFE_6767;
		default:
			return LIFE_RULES;
	}
}

static int
Array2Rule(int array)
{
	switch (array) {
		case LIFE_4555:
			return 4555;
		case LIFE_5766:
			return 5766;
		case LIFE_5655:
			return 5655;
		case LIFE_6767:
			return 6767;
		default:
			return 0;
	}
}

/*--- list ---*/
/* initialise the state of all cells to OFF */
static void
Init3D(life3dstruct * lp)
{
	lp->ptrhead = lp->ptrend = NULL;
	lp->eraserhead.next = &lp->eraserend;
	lp->eraserend.prev = &lp->eraserhead;
}

static CellList *
NewCell(void)
{
	return ((CellList *) malloc(sizeof (CellList)));
}

/*-
 * Function that adds the cell (assumed live) at (x,y,z) onto the search
 * list so that it is scanned in future generations
 */
static void
AddToList(life3dstruct * lp, unsigned int x, unsigned int y, unsigned int z)
{
	CellList   *tmp;

	tmp = NewCell();
	tmp->x = x;
	tmp->y = y;
	tmp->z = z;
	if (lp->ptrhead == NULL) {
		lp->ptrhead = lp->ptrend = tmp;
		tmp->prev = NULL;
	} else {
		lp->ptrend->next = tmp;
		tmp->prev = lp->ptrend;
		lp->ptrend = tmp;
	}
	lp->ptrend->next = NULL;
}

static void
AddToEraseList(life3dstruct * lp, CellList * cell)
{
	cell->next = &lp->eraserend;
	cell->prev = lp->eraserend.prev;
	lp->eraserend.prev->next = cell;
	lp->eraserend.prev = cell;
}

static void
DelFromList(life3dstruct * lp, CellList * cell)
{
	if (cell != lp->ptrhead)
		cell->prev->next = cell->next;
	else {
		lp->ptrhead = cell->next;
		if (lp->ptrhead != NULL)
			lp->ptrhead->prev = NULL;
	}

	if (cell != lp->ptrend)
		cell->next->prev = cell->prev;
	else {
		lp->ptrend = cell->prev;
		if (lp->ptrend != NULL)
			lp->ptrend->next = NULL;
	}

	AddToEraseList(lp, cell);
}

static void
DelFromEraseList(CellList * cell)
{
	cell->next->prev = cell->prev;
	cell->prev->next = cell->next;
	(void) free((void *) cell);
}

/*--- memory ---*/
/*-
 * Simulate a large array by dynamically allocating 4x4x4 size cells when
 * needed.
 */
static void
MemInit(life3dstruct * lp)
{
	int         i;

	for (i = 0; i < BASESIZE; ++i) {
		if (lp->base[i] != NULL) {
			(void) free((void *) lp->base[i]);
			lp->base[i] = NULL;
		}
	}
	lp->memstart = 0;
}

static void
BaseOffset(life3dstruct * lp,
	   unsigned int x, unsigned int y, unsigned int z, int *b, int *o)
{
	*b = ((x & 0x7c) << 7) + ((y & 0x7c) << 2) + ((z & 0x7c) >> 2);
	*o = (x & 3) + ((y & 3) << 2) + ((z & 3) << 4);

	if (!lp->base[*b])
		lp->base[*b] = (unsigned char *) calloc(64, sizeof (unsigned char));
}

static int
GetMem(life3dstruct * lp, unsigned int x, unsigned int y, unsigned int z)
{
	int         b, o;

	if (lp->memstart)
		MemInit(lp);
	BaseOffset(lp, x, y, z, &b, &o);
	return lp->base[b][o];
}

static void
SetMem(life3dstruct * lp,
       unsigned int x, unsigned int y, unsigned int z, unsigned int val)
{
	int         b, o;

	if (lp->memstart)
		MemInit(lp);

	BaseOffset(lp, x, y, z, &b, &o);
	lp->base[b][o] = val;
}

static void
ChangeMem(life3dstruct * lp,
	  unsigned int x, unsigned int y, unsigned int z, unsigned int val)
{
	int         b, o;

	if (lp->memstart)
		MemInit(lp);
	BaseOffset(lp, x, y, z, &b, &o);
	lp->base[b][o] += val;
}

static void
ClearMem(life3dstruct * lp)
{
	int         i, j, count;

	for (i = 0; i < BASESIZE; ++i)
		if (lp->base[i] != NULL) {
			for (count = j = 0; j < 64 && count == 0; ++j)
				if (CellState3D(lp->base[i][j]))
					++count;
			if (!count) {
				(void) free((void *) lp->base[i]);
				lp->base[i] = NULL;
			}
		}
}


/*-
 * This routine increments the values stored in the 27 cells centred on
 * (x,y,z) Note that the offset() macro implements wrapping - the world is a 
 * torus.
 */
static void
IncrementNbrs3D(life3dstruct * lp, CellList * cell)
{
	int         xc, yc, zc, x, y, z;

	xc = cell->x;
	yc = cell->y;
	zc = cell->z;
	for (z = zc - 1; z != zc + 2; ++z)
		for (y = yc - 1; y != yc + 2; ++y)
			for (x = xc - 1; x != xc + 2; ++x)
				if (x != xc || y != yc || z != zc)
					ChangeMem(lp,
						  (unsigned int) x, (unsigned int) y, (unsigned int) z, 1);
}

static void
End3D(life3dstruct * lp)
{
	CellList   *ptr;

	while (lp->ptrhead != NULL) {
		SetMem(lp, lp->ptrhead->x, lp->ptrhead->y, lp->ptrhead->z, OFF);
		DelFromList(lp, lp->ptrhead);
	}
	ptr = lp->eraserhead.next;
	while (ptr != &lp->eraserend) {
		DelFromEraseList(ptr);
		ptr = lp->eraserhead.next;
	}
	MemInit(lp);
}

static void
RunLife3D(life3dstruct * lp)
{
	unsigned int x, y, z, xc, yc, zc;
	int         c;
	CellList   *ptr, *ptrnextcell;

	/* Step 1 - Add 1 to all neighbours of living cells. */
	ptr = lp->ptrhead;
	while (ptr != NULL) {
		IncrementNbrs3D(lp, ptr);
		ptr = ptr->next;
	}

	/* Step 2 - Scan world and implement Survival rules. We have a list of live
	 * cells, so do the following:
	 * Start at the END of the list and work backwards (so we don't have to worry
	 * about scanning newly created cells since they are appended to the end) and
	 * for every entry, scan its neighbours for new live cells. If found, add them
	 * to the end of the list. If the centre cell is dead, unlink it.
	 * Make sure we do not append multiple copies of cells.
	 */
	ptr = lp->ptrend;
	while (ptr != NULL) {
		ptrnextcell = ptr->prev;
		xc = ptr->x;
		yc = ptr->y;
		zc = ptr->z;
		for (z = zc - 1; z != zc + 2; ++z)
			for (y = yc - 1; y != yc + 2; ++y)
				for (x = xc - 1; x != xc + 2; ++x)
					if (x != xc || y != yc || z != zc)
						if ((c = GetMem(lp, x, y, z))) {
							if (CellState3D(c) == OFF) {
								if (CellNbrs3D(c) >= lp->birth_min &&
								    CellNbrs3D(c) <= lp->birth_max)
									SetList3D(x, y, z);
								else
									Reset3D(x, y, z);
							}
						}
		c = GetMem(lp, xc, yc, zc);
		if (CellNbrs3D(c) < lp->living_min || CellNbrs3D(c) > lp->living_max) {
			SetMem(lp, ptr->x, ptr->y, ptr->z, OFF);
			DelFromList(lp, ptr);
		} else
			Set3D(xc, yc, zc);
		ptr = ptrnextcell;
	}
	ClearMem(lp);
}

#if 0
static int
CountCells3D(life3dstruct * lp)
{
	CellList   *ptr;
	int         count = 0;

	ptr = lp->ptrhead;
	while (ptr != NULL) {
		++count;
		ptr = ptr->next;
	}
	return count;
}

void
DisplayList(life3dstruct * lp)
{
	CellList   *ptr;
	int         count = 0;

	ptr = lp->ptrhead;
	while (ptr != NULL) {
		(void) printf("(%x)=[%d,%d,%d] ", (int) ptr, ptr->x, ptr->y, ptr->z);
		ptr = ptr->next;
		++count;
	}
	(void) printf("Living cells = %d\n", count);
}

#endif

static void
RandomSoup(ModeInfo * mi, int n, int v)
{
	life3dstruct *lp = &life3ds[MI_SCREEN(mi)];
	int         x, y, z;

	v /= 2;
	if (v < 1)
		v = 1;
	for (z = lp->nstacks / 2 - v; z < lp->nstacks / 2 + v; ++z)
		for (y = lp->nrows / 2 - v; y < lp->nrows / 2 + v; ++y)
			for (x = lp->ncolumns / 2 - v; x < lp->ncolumns / 2 + v; ++x)
				if (NRAND(100) < n)
					SetList3D(x, y, z);
	if (MI_WIN_IS_VERBOSE(mi))
		(void) fprintf(stdout, "random pattern\n");
}

static void
GetPattern(ModeInfo * mi, int pattern_rule, int pattern)
{
	life3dstruct *lp = &life3ds[MI_SCREEN(mi)];
	int         x, y, z;
	char       *patptr = NULL;

	switch (pattern_rule) {
		case LIFE_4555:
			patptr = &patterns_4555[pattern][0];
			break;
		case LIFE_5766:
			patptr = &patterns_5766[pattern][0];
			break;
		case LIFE_5655:
			patptr = &patterns_5655[pattern][0];
			break;
		case LIFE_6767:
			patptr = &patterns_6767[pattern][0];
			break;
	}
	while ((x = *patptr++) != 127) {
		y = *patptr++;
		z = *patptr++;
		x += lp->ncolumns / 2;
		y += lp->nrows / 2;
		z += lp->nstacks / 2;
		if (x >= 0 && y >= 0 && z >= 0 &&
		    x < lp->ncolumns && y < lp->nrows && z < lp->nstacks)
			SetList3D(x, y, z);
	}
	if (MI_WIN_IS_VERBOSE(mi))
		(void) fprintf(stdout, "table number %d\n", pattern);
}

static void
NewViewpoint(life3dstruct * lp, double x, double y, double z)
{
	double      k, l, d1, d2;

	k = x * x + y * y;
	l = sqrt(k + z * z);
	k = sqrt(k);
	d1 = (EyeToScreen / HalfScreenD);
	d2 = EyeToScreen / (HalfScreenD * lp->height / lp->width);
	lp->A = d1 * l * (lp->width / 2) / k;
	lp->B = l * l;
	lp->C = d2 * (lp->height / 2) / k;
	lp->F = k * k;
}

static void
NewPoint(life3dstruct * lp, double x, double y, double z,
	 register XPoint * cubepts)
{
	double      p1, E;

	p1 = x * lp->vx + y * lp->vy;
	E = lp->B - p1 - z * lp->vz;
	cubepts->x = (int) (lp->width / 2 - lp->A * (lp->vx * y - lp->vy * x) / E);
	cubepts->y = (int) (lp->height / 2 - lp->C * (z * lp->F - lp->vz * p1) / E);
}


/* Chain together all cells that are at the same distance. These * cannot
   mutually overlap. */
static void
SortList(life3dstruct * lp)
{
	short       dist;
	double      d, x, y, z, rsize;
	int         i, r;
	XPoint      point;
	CellList   *ptr;

	for (i = 0; i < NBUCKETS; ++i)
		lp->buckethead[i] = lp->bucketend[i] = NULL;

	/* Calculate distances and re-arrange pointers to chain off buckets */
	ptr = lp->ptrhead;
	while (ptr != NULL) {

		x = (double) ptr->x - lp->ox;
		y = (double) ptr->y - lp->oy;
		z = (double) ptr->z - lp->oz;
		d = Distance(lp->vx, lp->vy, lp->vz, x, y, z);
		if (lp->vx * (lp->vx - x) + lp->vy * (lp->vy - y) +
		    lp->vz * (lp->vz - z) > 0 && d > 1.5)
			ptr->visible = 1;
		else
			ptr->visible = 0;

		ptr->dist = (short) d;
		dist = (short) (d * BUCKETSIZE);
		if (dist > NBUCKETS - 1)
			dist = NBUCKETS - 1;

		if (lp->buckethead[dist] == NULL) {
			lp->buckethead[dist] = lp->bucketend[dist] = ptr;
			ptr->priority = NULL;
		} else {
			lp->bucketend[dist]->priority = ptr;
			lp->bucketend[dist] = ptr;
			lp->bucketend[dist]->priority = NULL;
		}
		ptr = ptr->next;
	}

	/* Check for invisibility */
	rsize = 0.47 * lp->width / ((double) HalfScreenD * 2);
	i = lp->azm;
	if (i < 0)
		i = -i;
	i = i % RT_ANGLE;
	if (i > HALFRT_ANGLE)
		i = RT_ANGLE - i;
	rsize /= cos(i * IP);

	lp->visible = 0;
	for (i = 0; i < NBUCKETS; ++i)
		lp->buckethead[i] = lp->bucketend[i] = NULL;

	/* Calculate distances and re-arrange pointers to chain off buckets */
	ptr = lp->ptrhead;
	while (ptr != NULL) {

		x = (double) ptr->x - lp->ox;
		y = (double) ptr->y - lp->oy;
		z = (double) ptr->z - lp->oz;
		d = Distance(lp->vx, lp->vy, lp->vz, x, y, z);
		if (lp->vx * (lp->vx - x) + lp->vy * (lp->vy - y) +
		    lp->vz * (lp->vz - z) > 0 && d > 1.5)
			ptr->visible = 1;
		else
			ptr->visible = 0;

		ptr->dist = (short) d;
		dist = (short) (d * BUCKETSIZE);
		if (dist > NBUCKETS - 1)
			dist = NBUCKETS - 1;

		if (lp->buckethead[dist] == NULL) {
			lp->buckethead[dist] = lp->bucketend[dist] = ptr;
			ptr->priority = NULL;
		} else {
			lp->bucketend[dist]->priority = ptr;
			lp->bucketend[dist] = ptr;
			lp->bucketend[dist]->priority = NULL;
		}
		ptr = ptr->next;
	}

	/* Check for invisibility */
	rsize = 0.47 * lp->width / ((double) HalfScreenD * 2);
	i = lp->azm;
	if (i < 0)
		i = -i;
	i = i % RT_ANGLE;
	if (i > HALFRT_ANGLE)
		i = RT_ANGLE - i;
	rsize /= cos(i * IP);

	lp->visible = 0;
	for (i = 0; i < NBUCKETS; ++i)
		if (lp->buckethead[i] != NULL) {
			ptr = lp->buckethead[i];
			while (ptr != NULL) {
				if (ptr->visible) {
					x = (double) ptr->x - lp->ox;
					y = (double) ptr->y - lp->oy;
					z = (double) ptr->z - lp->oz;
					NewPoint(lp, x, y, z, &point);

					r = (int) (rsize * (double) EyeToScreen / (double) ptr->dist);
					if (point.x + r >= 0 && point.y + r >= 0 &&
					    point.x - r < lp->width && point.y - r < lp->height)
						lp->visible = 1;
				}
				ptr = ptr->priority;
			}
		}
}

static void
DrawFace(ModeInfo * mi, int color, XPoint * cubepts,
	 int p1, int p2, int p3, int p4)
{
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	life3dstruct *lp = &life3ds[MI_SCREEN(mi)];
	XPoint      facepts[5];

	facepts[0] = cubepts[p1];
	facepts[1] = cubepts[p2];
	facepts[2] = cubepts[p3];
	facepts[3] = cubepts[p4];
	facepts[4] = cubepts[p1];

	if (!lp->wireframe) {
		XSetForeground(display, gc, lp->color[color]);
		XFillPolygon(display, MI_WINDOW(mi), gc, facepts, 4,
			     Convex, CoordModeOrigin);
	}
	if (color == BLACK || (MI_NPIXELS(mi) <= 2 && !lp->wireframe))
		XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
	else
		XSetForeground(display, gc, MI_WIN_WHITE_PIXEL(mi));
	XDrawLines(display, MI_WINDOW(mi), gc, facepts, 5, CoordModeOrigin);
}

#define LEN 0.45
#define LEN2 0.9

static int
DrawCube(ModeInfo * mi, CellList * cell)
{
	life3dstruct *lp = &life3ds[MI_SCREEN(mi)];
	XPoint      cubepts[8];	/* screen coords for point */
	int         i = 0, out;
	unsigned int mask;
	double      x, y, z;
	double      dx, dy, dz;

	x = (double) cell->x - lp->ox;
	y = (double) cell->y - lp->oy;
	z = (double) cell->z - lp->oz;
	out = 0;
	for (dz = z - LEN; dz <= z + LEN2; dz += LEN2)
		for (dy = y - LEN; dy <= y + LEN2; dy += LEN2)
			for (dx = x - LEN; dx <= x + LEN2; dx += LEN2) {
				NewPoint(lp, dx, dy, dz, &cubepts[i]);
				if (cubepts[i].x < 0 || cubepts[i].x >= lp->width ||
				    cubepts[i].y < 0 || cubepts[i].y >= lp->height)
					++out;
				++i;
			}
	if (out == 8)
		return (0);

	if (cell->visible)
		mask = 0xFFFF;
	else
		mask = 0x0;

	/* Only draw those faces that are visible */
	dx = lp->vx - x;
	dy = lp->vy - y;
	dz = lp->vz - z;
	if (dz > LEN)
		DrawFace(mi, (int) (BLUE & mask), cubepts, 4, 5, 7, 6);
	else if (dz < -LEN)
		DrawFace(mi, (int) (BLUE & mask), cubepts, 0, 1, 3, 2);
	if (dx > LEN)
		DrawFace(mi, (int) (GREEN & mask), cubepts, 1, 3, 7, 5);
	else if (dx < -LEN)
		DrawFace(mi, (int) (GREEN & mask), cubepts, 0, 2, 6, 4);
	if (dy > LEN)
		DrawFace(mi, (int) (RED & mask), cubepts, 2, 3, 7, 6);
	else if (dy < -LEN)
		DrawFace(mi, (int) (RED & mask), cubepts, 0, 1, 5, 4);
	return (1);
}

static void
DrawScreen(ModeInfo * mi)
{
	life3dstruct *lp = &life3ds[MI_SCREEN(mi)];
	CellList   *ptr;
	CellList   *eraserptr;
	int         i;

	SortList(lp);

	/* Erase dead cubes */
	eraserptr = lp->eraserhead.next;
	while (eraserptr != &lp->eraserend) {
		eraserptr->visible = 0;
		(void) DrawCube(mi, eraserptr);
		DelFromEraseList(eraserptr);
		eraserptr = lp->eraserhead.next;
	}

	/* draw furthest cubes first */
	for (i = NBUCKETS - 1; i >= 0; --i) {
		ptr = lp->buckethead[i];
		while (ptr != NULL) {
			/*if (ptr->visible) */
			/* v += */ (void) DrawCube(mi, ptr);
			ptr = ptr->priority;
			/* ++count; */
		}
	}
#if 0
	{
		int         count = 0, v = 0;


		(void) printf("Pop=%-4d  Viewpoint (%3d,%3d,%3d)  Origin (%3d,%3d,%3d)  Mode %dx%d\
(%d,%d) %d\n",
		     count, (int) (lp->vx + lp->ox), (int) (lp->vy + lp->oy),
			      (int) (lp->vz + lp->oz), (int) lp->ox, (int) lp->oy, (int) lp->oz,
			      lp->width, lp->height, lp->alt, lp->azm, v);
	}
#endif
}

static void
shooter(life3dstruct * lp)
{
	int         hsp, vsp, asp, hoff = 1, voff = 1, aoff = 1, r, c2,
	            r2, s2;

	/* Generate the glider at the edge of the screen */
#define V 10
#define V2 (V/2)
	c2 = lp->ncolumns / 2;
	r2 = lp->nrows / 2;
	s2 = lp->nstacks / 2;
	r = NRAND(3);
	if (!r) {
		hsp = NRAND(V2) + c2 - V2 / 2;
		vsp = (LRAND() & 1) ? r2 - V : r2 + V;
		asp = (LRAND() & 1) ? s2 - V : s2 + V;
		if (asp > s2)
			aoff = -1;
		if (vsp > r2)
			voff = -1;
		if (hsp > c2)
			hoff = -1;
		if (lp->rule3d == 4555) {
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 0 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 3 * hoff, vsp + 0 * voff, asp + 0 * aoff);
			SetList3D(hsp + 3 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 1 * aoff);
		} else if (lp->rule3d == 5766) {
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 2 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 2 * aoff);
		}
	} else if (r == 1) {
		hsp = (LRAND() & 1) ? c2 - V : c2 + V;
		vsp = (LRAND() & 1) ? r2 - V : r2 + V;
		asp = NRAND(V2) + s2 - V2 / 2;
		if (asp > s2)
			aoff = -1;
		if (vsp > r2)
			voff = -1;
		if (hsp > c2)
			hoff = -1;
		if (lp->rule3d == 4555) {
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 3 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 3 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 2 * aoff);
		} else if (lp->rule3d == 5766) {
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 2 * voff, asp + 1 * aoff);
		}
	} else {
		hsp = (LRAND() & 1) ? c2 - V : c2 + V;
		vsp = NRAND(V2) + r2 - V2 / 2;
		asp = (LRAND() & 1) ? s2 - V : s2 + V;
		if (asp > s2)
			aoff = -1;
		if (vsp > r2)
			voff = -1;
		if (hsp > c2)
			hoff = -1;
		if (lp->rule3d == 4555) {
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 0 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 3 * voff, asp + 0 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 3 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 1 * aoff);
		} else if (lp->rule3d == 5766) {
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 2 * aoff);
		}
	}
}

static void
initrule(ModeInfo * mi)
{
	life3dstruct *lp = &life3ds[MI_SCREEN(mi)];

	if (MI_WIN_IS_VERBOSE(mi))
		(void) fprintf(stdout, "rule3d %d\n", lp->rule3d);
	lp->living_min = lp->rule3d / 1000;
	lp->living_max = (lp->rule3d / 100) % 10;
	lp->birth_min = (lp->rule3d / 10) % 10;
	lp->birth_max = lp->rule3d % 10;
}

void
init_life3d(ModeInfo * mi)
{
	life3dstruct *lp;
	int         i, pattern_rule, npats;

	if (life3ds == NULL) {
		if ((life3ds = (life3dstruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (life3dstruct))) == NULL)
			return;
	}
	lp = &life3ds[MI_SCREEN(mi)];

	lp->generation = 0;

	lp->rule3d = rule3d;
	if (rule3d == 0)	/* All rules with patterns */
		lp->rule3d = Array2Rule(NRAND(LIFE_RULES));
	else if (rule3d == 1)	/* All rules with known gliders */
		lp->rule3d = Array2Rule(NRAND(LIFE_GLIDERS));
	if (lp->rule3d != rule3d || !lp->initialized) {
		/*
		 * This stuff is not good for rules above 9 cubes but it is unlikely that
		 * these modes would be much good anyway....
		 * Rules should not have to be contiguous ...  that is, it should be
		 * possible to have rules where cells will live if there are 5 or 7
		 * (not 6) cells around them.   These rules are not general enough
		 * to allow for that.
		 */
		initrule(mi);
	}
	if (!lp->initialized) {
		lp->dist = 50.0 /*30.0 */ ;
		lp->alt = 20 /*30 */ ;
		lp->azm = 10 /*30 */ ;
		lp->ncolumns = MAXCOLUMNS;
		lp->nrows = MAXROWS;
		lp->nstacks = MAXSTACKS;
		lp->ox = lp->ncolumns / 2;
		lp->oy = lp->nrows / 2;
		lp->oz = lp->nstacks / 2;
		lp->wireframe = 0;
		lp->initialized = 1;
		Init3D(lp);
	} else
		End3D(lp);
	lp->color[0] = MI_WIN_BLACK_PIXEL(mi);
	if (MI_NPIXELS(mi) > 2) {
		i = NRAND(3);

		lp->color[i + 1] = MI_PIXEL(mi, NRAND(MI_NPIXELS(mi) / COLORBASE));
		lp->color[(i + 1) % 3 + 1] = MI_PIXEL(mi,
					  NRAND(MI_NPIXELS(mi) / COLORBASE) +
						 MI_NPIXELS(mi) / COLORBASE);
		lp->color[(i + 2) % 3 + 1] = MI_PIXEL(mi,
					  NRAND(MI_NPIXELS(mi) / COLORBASE) +
					     2 * MI_NPIXELS(mi) / COLORBASE);
	} else
		lp->color[1] = lp->color[2] = lp->color[3] = MI_WIN_WHITE_PIXEL(mi);
	lp->color[4] = MI_WIN_WHITE_PIXEL(mi);
	lp->width = MI_WIN_WIDTH(mi);
	lp->height = MI_WIN_HEIGHT(mi);
	lp->memstart = 1;
	/*lp->tablesMade = 0; */

	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));

	if (lp->alt > 89)
		lp->alt = 89;
	else if (lp->alt < -89)
		lp->alt = -89;
	/* Calculate viewpoint */
	lp->vx = (sin(lp->azm * IP) * cos(lp->alt * IP) * lp->dist);
	lp->vy = (cos(lp->azm * IP) * cos(lp->alt * IP) * lp->dist);
	lp->vz = (sin(lp->alt * IP) * lp->dist);
	NewViewpoint(lp, lp->vx, lp->vy, lp->vz);

	pattern_rule = Rule2Array(lp->rule3d);
	if (pattern_rule < LIFE_RULES)
		npats = npatterns[pattern_rule];
	else
		npats = 0;
	lp->pattern = NRAND(npats + 2);
	if (lp->pattern >= npats)
		RandomSoup(mi, 30, 10);
	else
		GetPattern(mi, pattern_rule, lp->pattern);

	DrawScreen(mi);
}

void
draw_life3d(ModeInfo * mi)
{
	life3dstruct *lp = &life3ds[MI_SCREEN(mi)];

	RunLife3D(lp);
	DrawScreen(mi);

	if (++lp->generation > MI_CYCLES(mi) || !lp->visible) {
		/*CountCells3D(lp) == 0) */
		init_life3d(mi);
	}
	/*
	 * generate a randomized shooter aimed roughly toward the center of the
	 * screen after batchcount.
	 */

	if (lp->generation && lp->generation %
	    ((MI_BATCHCOUNT(mi) < 0) ? 1 : MI_BATCHCOUNT(mi)) == 0)
		shooter(lp);
}

void
release_life3d(ModeInfo * mi)
{
	if (life3ds != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			life3dstruct *lp = &life3ds[screen];

			End3D(lp);
		}
		(void) free((void *) life3ds);
		life3ds = NULL;
	}
}

void
refresh_life3d(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}

void
change_life3d(ModeInfo * mi)
{
	life3dstruct *lp = &life3ds[MI_SCREEN(mi)];
	int         npats, pattern_rule;

	lp->generation = 0;

	End3D(lp);
	/*lp->tablesMade = 0; */

	XClearWindow(MI_DISPLAY(mi), MI_WINDOW(mi));

	lp->pattern = lp->pattern + 1;
	pattern_rule = Rule2Array(lp->rule3d);
	if (pattern_rule < LIFE_RULES)
		npats = npatterns[pattern_rule];
	else
		npats = 0;
	if (lp->pattern >= npats + 2) {
		lp->pattern = 0;
		if (rule3d == 0) {
			pattern_rule++;
			if (pattern_rule >= LIFE_RULES)
				pattern_rule = 0;
			lp->rule3d = Array2Rule(pattern_rule);
			initrule(mi);
		} else if (rule3d == 1) {
			pattern_rule++;
			if (pattern_rule >= LIFE_GLIDERS)
				pattern_rule = 0;
			lp->rule3d = Array2Rule(pattern_rule);
			initrule(mi);
		}
	}
	if (lp->pattern >= npats)
		RandomSoup(mi, 30, 10);
	else
		GetPattern(mi, pattern_rule, lp->pattern);

	DrawScreen(mi);
}
