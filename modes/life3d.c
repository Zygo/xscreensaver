/* -*- Mode: C; tab-width: 4 -*- */
/* life3d --- Extension to Conway's game of Life, Carter Bays' S45/B5 3d life */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)life3d.c	5.07 2003/01/08 xlockmore";

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
 * 18-Apr-2004: Added shooting gliders for S567B6
 * 13-Mar-2004: Added references
 * 28-Apr-2003: Randomized "rotation" of life form
 * 25-Jan-2003: Spawned a life3d.h
 * 19-Jan-2003: added more gliders from http://www.cse.sc.edu/~bays
 * 08-Jan-2003: Double buffering
 * 01-Nov-2000: Allocation checks
 * 10-May-1997: Compatible with xscreensaver
 * 18-Apr-1997: Memory leak fixed by Tom Schmidt <tschmidt@micron.com>
 * 12-Mar-1995: added LIFE_S567B6 compile-time option
 * 12-Feb-1995: shooting gliders added
 * 07-Dec-1994: used life.c and a DOS version of 3dlife
 * Copyright 1993 Anthony Wesley awesley@canb.auug.org.au found at
 * life.anu.edu.au /pub/complex_systems/alife/3DLIFE.ZIP
 *
 *
 * References:
 * Dewdney, A.K., "The Armchair Universe, Computer Recreations from the
 *   Pages of Scientific American Magazine", W.H. Freedman and Company,
 *   New York, 1988 (February 1987 p 16)
 * Bays, Carter, "The Game of Three Dimensional Life", 86/11/20
 *   with (latest?) update from 87/2/1 http://www.cse.sc.edu/~bays/
 * Meeker, Lee Earl, "Four Dimensional Cellular Automata and the Game
 *   of Life" 1998 http://home.sc.rr.com/lmeeker/Lee/Home.html
 */

#ifdef STANDALONE
#define MODE_life3d
#define PROGCLASS "Life3D"
#define HACK_INIT init_life3d
#define HACK_DRAW draw_life3d
#define life3d_opts xlockmore_opts
#define DEFAULTS "*delay: 1000000 \n" \
 "*count: 35 \n" \
 "*cycles: 85 \n" \
 "*ncolors: 200 \n" \
 "*wireframe: False \n" \
 "*fullrandom: False \n" \
 "*verbose: False \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */
#include "iostuff.h"

#ifdef MODE_life3d
#define LIFE_NAMES 1
#include "life3d.h"

#ifdef LIFE_NAMES
#define DEF_LABEL "True"
#define FONT_HEIGHT 19
#define FONT_WIDTH 15
#define FONT_LENGTH 20
#endif
#define DEF_SERIAL "False"

#if 1
#define DEF_RULE  "G"		/* All rules with gliders */
#else
#define DEF_RULE  "P"		/* All rules with known patterns */
#define DEF_RULE  "S45/B5"	/* "B5/S45" */
#define DEF_RULE  "S567/B6"	/* "B6/S567" */
#define DEF_RULE  "S56/B5"	/* "B5/S56" */
#define DEF_RULE  "S678/B5"	/* "B5/S678" */
#define DEF_RULE  "S67/B67"	/* "B67/S67" */
/* There are no known gliders for S67/B67 */
#endif

static char *rule;
static char *lifefile;
#ifdef LIFE_NAMES
static Bool label;
#endif
static Bool serial;

static XrmOptionDescRec opts[] =
{
#ifdef LIFE_NAMES
	{(char *) "-label", (char *) ".life3d.label", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+label", (char *) ".life3d.label", XrmoptionNoArg, (caddr_t) "off"},
#endif
	{(char *) "-rule", (char *) ".life3d.rule", XrmoptionSepArg, (caddr_t) NULL},
	{(char *) "-lifefile", (char *) ".life3d.lifefile", XrmoptionSepArg, (caddr_t) NULL},
	{(char *) "-serial", (char *) ".life3d.serial", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+serial", (char *) ".life3d.serial", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
#ifdef LIFE_NAMES
	{(void *) & label, (char *) "label", (char *) "Label", (char *) DEF_LABEL, t_Bool},
#endif
	{(void *) & rule, (char *) "rule", (char *) "Rule", (char *) DEF_RULE, t_String},
	{(void *) & lifefile, (char *) "lifefile", (char *) "LifeFile", (char *) "", t_String},
	{(void *) & serial, (char *) "serial", (char *) "Serial", (char *) DEF_SERIAL, t_Bool}

};
static OptionStruct desc[] =
{
#ifdef LIFE_NAMES
	{(char *) "-/+label", (char *) "turn on/off name labeling"},
#endif
	{(char *) "-rule string", (char *) "S<survial_neighborhood>/B<birth_neighborhood> parameters"},
	{(char *) "-lifefile file", (char *) "life file"},
	{(char *) "-/+serial", (char *) "turn on/off picking of sequential patterns"}
};

ModeSpecOpt life3d_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   life3d_description =
{"life3d", "init_life3d", "draw_life3d", "release_life3d",
 "refresh_life3d", "change_life3d", (char *) NULL, &life3d_opts,
 1000000, 35, 85, 1, 64, 1.0, "",
 "Shows Bays' game of 3D Life", 0, NULL};

#endif

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

#define SetList3D(x,y,z) if (!SetMem(lp,(unsigned int)x,(unsigned int)y,(unsigned int)z,ON)) return False; \
if (!AddToList(lp,(unsigned int)x,(unsigned int)y,(unsigned int)z)) return False

#define CellState3D(c) ((c)&ON)
#define CellNbrs3D(c) ((c)&0x1f)	/* 26 <= 31 */

#define EyeToScreen 72.0	/* distance from eye to screen */
#define HalfScreenD 14.0	/* 1/2 the diameter of screen */
#define BUCKETSIZE 10
#define NBUCKETS ((MAXCOLUMNS+MAXROWS+MAXSTACKS)*BUCKETSIZE)
#define Distance(x1,y1,z1,x2,y2,z2) sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2)+(z1-z2)*(z1-z2))

#define IP (M_PI / 180.0)

#define COLORBASE 3
#define COLORS (COLORBASE + 2)

#define BLACK 0
#define RED   1
#define GREEN 2
#define BLUE  3
#define WHITE 4

typedef struct _CellList {
	unsigned char x, y, z;	/* Location in world coordinates */
	char        visible;	/* 1 if the cube is to be displayed */
	short       dist;	/* dist from cell to eye */
	struct _CellList *next;	/* pointer to next entry on linked list */
	struct _CellList *prev;
	struct _CellList *priority;
} CellList;

typedef struct {
	Bool        painted;
	paramstruct param;
	int         pattern, patterned_rule;
	int         ox, oy, oz;	/* origin */
	double      vx, vy, vz;	/* viewpoint */
	int         generation;
	int         ncolumns, nrows, nstacks;
	int         memstart;
	char        visible;
	int         noChangeCount;
	unsigned char *base[BASESIZE];
	double      A, B, C, F;
	int         width, height;
	unsigned long colors[COLORS];
	double      azm;
	double      metaAlt, metaAzm, metaDist;
	CellList   *ptrhead, *ptrend, eraserhead, eraserend;
	CellList   *buckethead[NBUCKETS], *bucketend[NBUCKETS];
	Bool        wireframe;
	Pixmap      dbuf;
	int         labelOffsetX, labelOffsetY;
	char        ruleString[80], nameString[80];
} life3dstruct;

static life3dstruct *life3ds = (life3dstruct *) NULL;

static paramstruct input_param;
static Bool allPatterns = False, allGliders = False;
static char *filePattern = (char *) NULL;

static int
codeToPatternedRule(paramstruct param)
{
	unsigned int i;

	for (i = 0; i < LIFE_RULES; i++)
		if (param_rules[i].survival == param.survival &&
		    param_rules[i].birth == param.birth)
			return i;
	return LIFE_RULES;
}

static void
copyFromPatternedRule(paramstruct * param, int patterned_rule)
{
	param->survival = param_rules[patterned_rule].survival;
	param->birth = param_rules[patterned_rule].birth;
}

static void
printRule(char * string, paramstruct param, Bool verbose)
{
	int         i = 1, l;

	string[0] = 'S';
	if (verbose)
		(void) fprintf(stdout, "rule (Survival/Birth neighborhood): ");
	for (l = 0; l <= MAXNEIGHBORS && l < 10; l++) {

		if (param.survival & (1 << l)) {
			(void) sprintf(&(string[i]), "%d", l);
			i++;
		}
	}
	(void) sprintf(&(string[i]), "/B");
	i += 2;
	for (l = 0; l <= MAXNEIGHBORS && l < 10; l++) {
		if (param.birth & (1 << l)) {
			(void) sprintf(&(string[i]), "%d", l);
			i++;
		}
	}
	string[i] = '\0';
	if (verbose)
	  (void) fprintf(stdout, "%s\nbinary rule: Survival 0x%X, Birth 0x%X\n",
		       string, param.survival, param.birth);
}

/*-
 * This stuff is not good for rules above 9 cubes but it is unlikely that
 * these modes would be much good anyway....  death assumed.
 */
static void
parseRule(ModeInfo * mi, char * string)
{
	int         n, l;
	char        serving = 0;
	static Bool found = False;

	if (found)
		return;
	input_param.survival = input_param.birth = 0;
	if (rule) {
		n = 0;
		while (rule[n]) {
			if (rule[n] == 'P' || rule[n] == 'p') {
				allPatterns = True;
				found = True;
				if (MI_IS_VERBOSE(mi))
					(void) fprintf(stdout, "rule: All rules with known patterns\n");
				return;
			} else if (rule[n] == 'G' || rule[n] == 'g') {
				allGliders = True;
				found = True;
				if (MI_IS_VERBOSE(mi))
					(void) fprintf(stdout, "rule: All rules with known gliders\n");
				return;
			} else if (rule[n] == 'S' || rule[n] == 'E' || rule[n] == 'L' || rule[n] == 's' || rule[n] == 'e' || rule[n] == 'l') {
				serving = 'S';
			} else if (rule[n] == 'B' || rule[n] == 'b') {
				serving = 'B';
			} else {
				l = rule[n] - '0';
				if (l >= 0 && l <= 9 /*&& l <= MAXNEIGHBORS */ ) {	/* no 10..27 */
					if (serving == 'S' || rule[n] == 's') {
						found = True;
						input_param.survival |= (1 << l);
					} else if (serving == 'B' || rule[n] == 'b') {
						found = True;
						input_param.birth |= (1 << l);
					}
				}
			}
			n++;
		}
	}
	if (!found || !(input_param.survival || input_param.birth)) {
		/* Default to Bays' rules if rule does not make sense */
		allGliders = True;
		found = True;
		if (MI_IS_VERBOSE(mi))
			(void) fprintf(stdout,
				"rule: Defaulting to all rules with known gliders\n");
		return;
	}
	printRule(string, input_param, MI_IS_VERBOSE(mi));
}

static void
parseFile(ModeInfo *mi)
{
	FILE       *file;
	static Bool done = False;
	int         firstx, firsty, x = 0, y = 0, z = 0, i = 0;
	int         c, cprev = ' ', size;
	char        line[256];

	if (done)
		return;
	done = True;
	if (MI_IS_FULLRANDOM(mi) || !lifefile || !*lifefile)
		return;
	if ((file = my_fopenSize(lifefile, "r", &size)) == NULL) {
		(void) fprintf(stderr, "could not read file \"%s\"\n", lifefile);
		return;
	}
	for (;;) {
		if (!fgets(line, 256, file)) {
			(void) fprintf(stderr, "could not read header of file \"%s\"\n",
				       lifefile);
			(void) fclose(file);
			return;
		}
		if (strncmp(line, "#P", (size_t) 2) == 0 &&
		    sscanf(line, "#P %d %d %d", &x, &y, &z) == 3)
			break;
	}
	c = getc(file);
	while (c != EOF && !(c == '0' || c == 'O' || c == '*' || c == '.')) {
		c = getc(file);
	}
	if (c == EOF || x <= -127 || y <= -127 || z <= -127 ||
	    x >= 127 || y >= 127 || z >= 127) {
		(void) fprintf(stderr, "corrupt file \"%s\" or file to large\n",
			       lifefile);
		(void) fclose(file);
		return;
	}
	firstx = x;
	firsty = y;
	if ((filePattern = (char *) malloc((3 * size) *
			 sizeof (char))) == NULL) {
		(void) fprintf(stderr, "not enough memory\n");
		(void) fclose(file);
	}

	while (c != EOF && x < 127 && y < 127 && z < 127 && i < 3 * size) {
		if (c == '0' || c == 'O' || c == '*') {
			filePattern[i++] = x++;
			filePattern[i++] = y;
			filePattern[i++] = z;
		} else if (c == '.') {
			x++;
		} else if (c == '\n') {
			if (cprev == '\n') {
				z++;
				y = firsty;
			} else {
				x = firstx;
				y++;
			}
		}
		cprev = c;
		c = getc(file);
	}
	(void) fclose(file);
	filePattern[i] = 127;
}

/*--- list ---*/
/* initialise the state of all cells to OFF */
static void
Init3D(life3dstruct * lp)
{
	lp->ptrhead = lp->ptrend = (CellList *) NULL;
	lp->eraserhead.next = &lp->eraserend;
	lp->eraserend.prev = &lp->eraserhead;
}

/*-
 * Function that adds the cell (assumed live) at (x,y,z) onto the search
 * list so that it is scanned in future generations
 */
static Bool
AddToList(life3dstruct * lp, unsigned int x, unsigned int y, unsigned int z)
{
	CellList   *tmp;

	if ((tmp = (CellList *) malloc(sizeof (CellList))) == NULL)
		return False;
	tmp->x = x;
	tmp->y = y;
	tmp->z = z;
	if (lp->ptrhead == NULL) {
		lp->ptrhead = lp->ptrend = tmp;
		tmp->prev = (struct _CellList *) NULL;
	} else {
		lp->ptrend->next = tmp;
		tmp->prev = lp->ptrend;
		lp->ptrend = tmp;
	}
	lp->ptrend->next = (struct _CellList *) NULL;
	return True;
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
	if (cell != lp->ptrhead) {
		cell->prev->next = cell->next;
	} else {
		lp->ptrhead = cell->next;
		if (lp->ptrhead != NULL)
			lp->ptrhead->prev = (struct _CellList *) NULL;
	}

	if (cell != lp->ptrend) {
		cell->next->prev = cell->prev;
	} else {
		lp->ptrend = cell->prev;
		if (lp->ptrend != NULL)
			lp->ptrend->next = (struct _CellList *) NULL;
	}

	AddToEraseList(lp, cell);
}

static void
DelFromEraseList(CellList * cell)
{
	cell->next->prev = cell->prev;
	cell->prev->next = cell->next;
	free(cell);
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
			free(lp->base[i]);
			lp->base[i] = (unsigned char *) NULL;
		}
	}
	lp->memstart = 0;
}

#define BASE_OFFSET(x,y,z,b,o) \
b = ((x & 0x7c) << 7) + ((y & 0x7c) << 2) + ((z & 0x7c) >> 2); \
o = (x & 3) + ((y & 3) << 2) + ((z & 3) << 4); \
if (lp->base[b] == NULL) {\
if ((lp->base[b] = (unsigned char *) calloc(64, sizeof (unsigned char))) == NULL) {return False;}}


static Bool
GetMem(life3dstruct * lp, unsigned int x, unsigned int y, unsigned int z,
		int *m)
{
	int         b, o;

	if (lp->memstart) {
		MemInit(lp);
	}
	BASE_OFFSET(x, y, z, b, o);
	*m = lp->base[b][o];
	return True;
}

static Bool
SetMem(life3dstruct * lp,
       unsigned int x, unsigned int y, unsigned int z, unsigned int val)
{
	int         b, o;

	if (lp->memstart) {
		MemInit(lp);
	}
	BASE_OFFSET(x, y, z, b, o);
	lp->base[b][o] = val;
	return True;
}

static Bool
ChangeMem(life3dstruct * lp,
		unsigned int x, unsigned int y, unsigned int z,
		unsigned int val)
{
	int         b, o;

	if (lp->memstart) {
		MemInit(lp);
	}
	BASE_OFFSET(x, y, z, b, o);
	lp->base[b][o] += val;
	return True;
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
				free(lp->base[i]);
				lp->base[i] = (unsigned char *) NULL;
			}
		}
}


/*-
 * This routine increments the values stored in the 27 cells centred on
 * (x,y,z) Note that the offset() macro implements wrapping - the world is a
 * 4d torus
 */
static Bool
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
					if (!ChangeMem(lp,
						  (unsigned int) x, (unsigned int) y, (unsigned int) z, 1))
						return False;
	return True;
}

static void
End3D(life3dstruct * lp)
{
	CellList   *ptr;

	while (lp->ptrhead != NULL) {
		/* Reset3D(lp->ptrhead->x, lp->ptrhead->y, lp->ptrhead->z); */
		DelFromList(lp, lp->ptrhead);
	}
	ptr = lp->eraserhead.next;
	while (ptr != &lp->eraserend) {
		DelFromEraseList(ptr);
		ptr = lp->eraserhead.next;
	}
	MemInit(lp);
}

static Bool
RunLife3D(life3dstruct * lp)
{
	unsigned int x, y, z, xc, yc, zc;
	int         c;
	CellList   *ptr, *ptrnextcell;
	Bool visible = False;

	/* Step 1 - Add 1 to all neighbours of living cells. */
	ptr = lp->ptrhead;
	while (ptr != NULL) {
		if (!IncrementNbrs3D(lp, ptr))
			return False;
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
					if (x != xc || y != yc || z != zc) {
						if (!GetMem(lp, x, y, z, &c))
							return False;
						if (c) {
							if (CellState3D(c) == OFF) {
								if (lp->param.birth & (1 << CellNbrs3D(c))) {
									visible = True;
									SetList3D(x, y, z);
								} else {
									if (!Reset3D(x, y, z))
										return False;

								}
							}
						}
					}
		if (!GetMem(lp, xc, yc, zc, &c))
			return False;
		if (lp->param.survival & (1 << CellNbrs3D(c))) {
			if (!Set3D(xc, yc, zc))
				return False;
		} else {
			if (!Reset3D(ptr->x, ptr->y, ptr->z))
				return False;
			DelFromList(lp, ptr);
		}
		ptr = ptrnextcell;
	}
	ClearMem(lp);
	if (visible)
		lp->noChangeCount = 0;
	else
		lp->noChangeCount++;
	return True;
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
		(void) printf("(%x)=[%d,%d,%d] ", (int) ptr,
			ptr->x, ptr->y, ptr->z);
		ptr = ptr->next;
		++count;
	}
	(void) printf("Living cells = %d\n", count);
}

#endif

static Bool
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
				if (NRAND(100) < n) {
					SetList3D(x, y, z);
				}
	(void) strcpy(lp->nameString, "random pattern");
	if (MI_IS_VERBOSE(mi)) {
		(void) fprintf(stdout, "%s\n", lp->nameString);
	}
	return True;
}

static Bool
GetPattern(ModeInfo * mi, int pattern_rule, int pattern)
{
	life3dstruct *lp = &life3ds[MI_SCREEN(mi)];
	int         x, y, z, orient, temp;
	char       *patptr = (char *) NULL;
#ifdef LIFE_NAMES
	int pat = 2 * pattern + 1;
	char * patstrg = (char *) "";
#else
	int pat = pattern;
#endif

	if (filePattern) {
		patptr = &filePattern[0];
	} else {
		switch (pattern_rule) {
			case LIFE_S45B5:
				patptr = &patterns_S45B5[pat][0];
#ifdef LIFE_NAMES
				patstrg = &patterns_S45B5[2 * pattern][0];
#endif
				break;
			case LIFE_S567B6:
				patptr = &patterns_S567B6[pat][0];
#ifdef LIFE_NAMES
				patstrg = &patterns_S567B6[2 * pattern][0];
#endif
				break;
			case LIFE_S56B5:
				patptr = &patterns_S56B5[pat][0];
#ifdef LIFE_NAMES
				patstrg = &patterns_S56B5[2 * pattern][0];
#endif
				break;
			case LIFE_S678B5:
				patptr = &patterns_S678B5[pat][0];
#ifdef LIFE_NAMES
				patstrg = &patterns_S678B5[2 * pattern][0];
#endif
				break;
			case LIFE_S67B67:
				patptr = &patterns_S67B67[pat][0];
#ifdef LIFE_NAMES
				patstrg = &patterns_S67B67[2 * pattern][0];
#endif
				break;
		}
#ifdef LIFE_NAMES
		(void) strcpy(lp->nameString, patstrg);
#endif
	}
#ifdef DEBUG
	orient = 0;
#else
	orient = NRAND(24);
#endif
	if (MI_IS_VERBOSE(mi) && !filePattern) {
#ifdef LIFE_NAMES
		(void) fprintf(stdout, "%s, ", patstrg);
#endif
		(void) fprintf(stdout, "table number %d\n", pattern);
	}
	while ((x = *patptr++) != 127) {
		y = *patptr++;
		z = *patptr++;
		if (orient >= 16) {
			temp = x;
			x = y;
			y = z;
			z = temp;
		} else if (orient >= 8) {
			temp = x;
			x = z;
			z = y;
			y = temp;
		}
		if (orient % 8 >= 4) {
			x = -x;
		}
		if (orient % 4 >= 2) {
			y = -y;
		}
		if (orient % 2) {
			z = -z;
		}

		x += lp->ncolumns / 2;
		y += lp->nrows / 2;
		z += lp->nstacks / 2;
		if (x >= 0 && y >= 0 && z >= 0 &&
		    x < lp->ncolumns && y < lp->nrows && z < lp->nstacks) {
			SetList3D(x, y, z);
		}
	}
	return True;
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
lissajous(life3dstruct * lp)
{
	double alt, azm, dist;

	alt = 30.0 * sin(lp->metaAlt * IP) + 45.0;
	lp->metaAlt += 1.123;
	if (lp->metaAlt >= 360.0)
		lp->metaAlt -= 360.0;
	if (lp->metaAlt < 0.0)
		lp->metaAlt += 360.0;
	azm = 30.0 * sin(lp->metaAzm * IP) + 45.0;
	lp->metaAzm += 0.987;
	if (lp->metaAzm >= 360.0)
		lp->metaAzm -= 360.0;
	if (lp->metaAzm < 0.0)
		lp->metaAzm += 360.0;
	dist = 10.0 * sin(lp->metaDist * IP) + 50.0;
	lp->metaDist += 1.0;
	if (lp->metaDist >= 360.0)
		lp->metaDist -= 360.0;
	if (lp->metaDist < 0.0)
		lp->metaDist += 360.0;
#if 0
	if (alt >= 90.0)
		alt = 90.0;
	else if (alt < -90.0)
		alt = -90.0;
#endif
	lp->azm = azm;
#ifdef DEBUG
	(void) printf("dist %g, alt %g, azm %g\n", dist, alt, azm);
#endif
	lp->vx = (sin(azm * IP) * cos(alt * IP) * dist);
	lp->vy = (cos(azm * IP) * cos(alt * IP) * dist);
	lp->vz = (sin(alt * IP) * dist);
	NewViewpoint(lp, lp->vx, lp->vy, lp->vz);
}

static void
NewPoint(life3dstruct * lp, double x, double y, double z,
	 register XPoint * cubepts)
{
	double      p1, E;

	p1 = x * lp->vx + y * lp->vy;
	E = lp->B - p1 - z * lp->vz;
	cubepts->x = (int) (lp->width / 2 -
		lp->A * (lp->vx * y - lp->vy * x) / E);
	cubepts->y = (int) (lp->height / 2 -
		lp->C * (z * lp->F - lp->vz * p1) / E);
}


/* Chain together all cells that are at the same distance.
 * These cannot mutually overlap. */
static void
SortList(life3dstruct * lp)
{
	short       dist;
	double      d, x, y, z, rsize;
	int         i, r;
	XPoint      point;
	CellList   *ptr;

	for (i = 0; i < NBUCKETS; ++i)
		lp->buckethead[i] = lp->bucketend[i] = (CellList *) NULL;

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
			ptr->priority = (struct _CellList *) NULL;
		} else {
			lp->bucketend[dist]->priority = ptr;
			lp->bucketend[dist] = ptr;
			lp->bucketend[dist]->priority =
				(struct _CellList *) NULL;
		}
		ptr = ptr->next;
	}

	/* Check for invisibility */
	rsize = 0.47 * lp->width / ((double) HalfScreenD * 2);
	i = (int) lp->azm;
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

	XSetForeground(display, gc, lp->colors[color]);
	if (!lp->wireframe) {
		XFillPolygon(display, lp->dbuf, gc, facepts, 4,
			Convex, CoordModeOrigin);
		if (color == BLACK || MI_NPIXELS(mi) <= 2) {
			XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		} else {
			XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
		}
	}
	XDrawLines(display, lp->dbuf, gc, facepts, 5, CoordModeOrigin);
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
				if (cubepts[i].x < 0 ||
				    cubepts[i].x >= lp->width ||
				    cubepts[i].y < 0 ||
				    cubepts[i].y >= lp->height)
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
	if (lp->wireframe) {
		if (dz <= LEN)
			DrawFace(mi, (int) (BLUE & mask), cubepts, 4, 5, 7, 6);
		else if (dz >= -LEN)
			DrawFace(mi, (int) (BLUE & mask), cubepts, 0, 1, 3, 2);
		if (dx <= LEN)
			DrawFace(mi, (int) (GREEN & mask), cubepts, 1, 3, 7, 5);
		else if (dx >= -LEN)
			DrawFace(mi, (int) (GREEN & mask), cubepts, 0, 2, 6, 4);
		if (dy <= LEN)
			DrawFace(mi, (int) (RED & mask), cubepts, 2, 3, 7, 6);
		else if (dy >= -LEN)
			DrawFace(mi, (int) (RED & mask), cubepts, 0, 1, 5, 4);
	}
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
	Display    *display = MI_DISPLAY(mi);
	GC          gc = MI_GC(mi);
	CellList   *ptr;
	CellList   *eraserptr;
	int         i;

	SortList(lp);

	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	XFillRectangle(display, lp->dbuf, gc, 0, 0,
		lp->width, lp->height);

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
	if (label) {
		int size = MAX(MIN(MI_WIDTH(mi), MI_HEIGHT(mi)) - 1, 1);
		if (size >= 10 * FONT_WIDTH) {
			/* hard code these to corners */
			XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
			XDrawString(display, lp->dbuf, gc,
				16 + lp->labelOffsetX,
				16 + lp->labelOffsetY + FONT_HEIGHT,
				lp->ruleString, strlen(lp->ruleString));
			XDrawString(display, lp->dbuf, gc,
				16 + lp->labelOffsetX,
				MI_HEIGHT(mi) - 16 -
			       	lp->labelOffsetY - FONT_HEIGHT / 2,
				lp->nameString, strlen(lp->nameString));
		}
         }
	XFlush(display);
	XCopyArea(display, lp->dbuf, MI_WINDOW(mi), gc,
		0, 0, lp->width, lp->height, 0, 0);

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

static Bool
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
		if (lp->patterned_rule == LIFE_S45B5) {
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
		} else if (lp->patterned_rule == LIFE_S567B6) {
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
		} else if (lp->patterned_rule == LIFE_S56B5) {
			SetList3D(hsp + 2 * hoff, vsp + 4 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 4 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 4 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 4 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 3 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 4 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 3 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 3 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 3 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 4 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 4 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 4 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 4 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 4 * aoff);
		} else if (lp->patterned_rule == LIFE_S678B5) {
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 3 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 0 * aoff);
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
		if (lp->patterned_rule == LIFE_S45B5) {
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
		} else if (lp->patterned_rule == LIFE_S567B6) {
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
		} else if (lp->patterned_rule == LIFE_S56B5) {
			SetList3D(hsp + 4 * hoff, vsp + 0 * voff, asp + 2 * aoff);
			SetList3D(hsp + 4 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 4 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 4 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 3 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 4 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 3 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 3 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 3 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 4 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 4 * voff, asp + 2 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 4 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 4 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 4 * voff, asp + 0 * aoff);
		} else if (lp->patterned_rule == LIFE_S678B5) {
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 3 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 2 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 0 * voff, asp + 2 * aoff);
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
		if (lp->patterned_rule == LIFE_S45B5) {
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
		} else if (lp->patterned_rule == LIFE_S567B6) {
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
		} else if (lp->patterned_rule == LIFE_S56B5) {
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 4 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 4 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 4 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 4 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 3 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 4 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 0 * voff, asp + 3 * aoff);
			SetList3D(hsp + 3 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 3 * hoff, vsp + 0 * voff, asp + 1 * aoff);
			SetList3D(hsp + 4 * hoff, vsp + 2 * voff, asp + 1 * aoff);
			SetList3D(hsp + 4 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 4 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 4 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 4 * hoff, vsp + 0 * voff, asp + 1 * aoff);
		} else if (lp->patterned_rule == LIFE_S678B5) {
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 2 * voff, asp + 1 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 0 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 2 * hoff, vsp + 3 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 1 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 1 * hoff, vsp + 2 * voff, asp + 0 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 1 * voff, asp + 0 * aoff);
			SetList3D(hsp + 0 * hoff, vsp + 2 * voff, asp + 0 * aoff);
		}
	}
	return True;
}

static void
free_life3d(Display *display, life3dstruct *lp)
{
	if (lp->eraserhead.next != NULL)
		End3D(lp);
        if (lp->dbuf != None) {
		XFreePixmap(display, lp->dbuf);
		lp->dbuf = None;
	}
}

void
init_life3d(ModeInfo * mi)
{
	Display *display = MI_DISPLAY(mi);
	life3dstruct *lp;
	int i, npats;

	if (life3ds == NULL) {
		if ((life3ds = (life3dstruct *) calloc(MI_NUM_SCREENS(mi),
				sizeof (life3dstruct))) == NULL)
			return;
	}
	lp = &life3ds[MI_SCREEN(mi)];

	lp->generation = 0;

	lp->labelOffsetX = NRAND(8);
	lp->labelOffsetY = NRAND(8);
	parseRule(mi, lp->ruleString);
	parseFile(mi);
	if (allPatterns) {
		lp->patterned_rule = NRAND(LIFE_RULES);
		copyFromPatternedRule(&lp->param, lp->patterned_rule);
		printRule(lp->ruleString, lp->param, MI_IS_VERBOSE(mi));
	} else if (allGliders) {
		lp->patterned_rule = NRAND(LIFE_GLIDERS);
		copyFromPatternedRule(&lp->param, lp->patterned_rule);
		printRule(lp->ruleString, lp->param, MI_IS_VERBOSE(mi));
	} else {
		lp->param.survival = input_param.survival;
		lp->param.birth = input_param.birth;
	}
	if (!lp->eraserhead.next) {
		lp->metaDist = (double) NRAND(360);
                lp->metaAlt = (double) NRAND(360);
                lp->metaAzm = (double) NRAND(360);
		lp->ncolumns = MAXCOLUMNS;
		lp->nrows = MAXROWS;
		lp->nstacks = MAXSTACKS;
		lp->ox = lp->ncolumns / 2;
		lp->oy = lp->nrows / 2;
		lp->oz = lp->nstacks / 2;

		Init3D(lp);
	} else {
		End3D(lp);
	}
	lp->colors[0] = MI_BLACK_PIXEL(mi);
	if (MI_NPIXELS(mi) > 2) {
		i = NRAND(3);

		lp->colors[i + 1] = MI_PIXEL(mi,
			NRAND(MI_NPIXELS(mi) / COLORBASE));
		lp->colors[(i + 1) % 3 + 1] = MI_PIXEL(mi,
			NRAND(MI_NPIXELS(mi) / COLORBASE) +
			MI_NPIXELS(mi) / COLORBASE);
		lp->colors[(i + 2) % 3 + 1] = MI_PIXEL(mi,
			NRAND(MI_NPIXELS(mi) / COLORBASE) +
			2 * MI_NPIXELS(mi) / COLORBASE);
	} else {
		lp->colors[1] = lp->colors[2] = lp->colors[3] =
			MI_WHITE_PIXEL(mi);
	}
	lp->colors[4] = MI_WHITE_PIXEL(mi);
	lp->width = MI_WIDTH(mi);
	lp->height = MI_HEIGHT(mi);
	lp->memstart = 1;
	lp->noChangeCount = False;
	/*lp->tablesMade = 0; */

	if (MI_IS_FULLRANDOM(mi)) {
		lp->wireframe = (NRAND(8) == 0);
	} else {
		lp->wireframe = MI_IS_WIREFRAME(mi);
	}

	MI_CLEARWINDOW(mi);
	lp->painted = False;

	lissajous(lp);

	lp->patterned_rule = codeToPatternedRule(lp->param);
	if ((unsigned) lp->patterned_rule < LIFE_RULES)
		npats = patterns_rules[lp->patterned_rule];
	else
		npats = 0;
	lp->pattern = NRAND(npats + 2);
	if (lp->pattern >= npats && !filePattern) {
		if (!RandomSoup(mi, 30, 10)) {
			if (lp->eraserhead.next != NULL)
				End3D(lp);
			return;
		}
	} else {
		if (!GetPattern(mi, lp->patterned_rule, lp->pattern)) {
			if (lp->eraserhead.next != NULL)
				End3D(lp);
			return;
		}
	}

	if (lp->dbuf != None) {
		XFreePixmap(display, lp->dbuf);
		lp->dbuf = None;
	}
	if ((lp->dbuf = XCreatePixmap(display, MI_WINDOW(mi),
		lp->width, lp->height, MI_DEPTH(mi))) == None) {
		free_life3d(display, lp);
		return;
	}

	DrawScreen(mi);
}

void
draw_life3d(ModeInfo * mi)
{
	life3dstruct *lp;

	if (life3ds == NULL)
		return;
	lp = &life3ds[MI_SCREEN(mi)];
	if (lp->eraserhead.next == NULL)
		return;

	if (!RunLife3D(lp)) {
		if (lp->eraserhead.next != NULL)
			End3D(lp);
		return;
	}
	lissajous(lp);
	if (lp->visible) /* kill static life also */
		DrawScreen(mi);

	MI_IS_DRAWN(mi) = True;

	if (++lp->generation > MI_CYCLES(mi) || !lp->visible || lp->noChangeCount >= 8) {
		/*CountCells3D(lp) == 0) */
		init_life3d(mi);
	} else
		lp->painted = True;

	/*
	 * generate a randomized shooter aimed roughly toward the center of the
	 * screen after batchcount.
	 */

	if (MI_COUNT(mi)) {
		if (lp->generation && lp->generation %
				((MI_COUNT(mi) < 0) ? 1 : MI_COUNT(mi)) == 0)
			if (!shooter(lp)) {
				if (lp->eraserhead.next != NULL)
					End3D(lp);
			}
	}
}

void
release_life3d(ModeInfo * mi)
{
	if (life3ds != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_life3d(MI_DISPLAY(mi), &life3ds[screen]);
		free(life3ds);
		life3ds = (life3dstruct *) NULL;
	}
}

void
refresh_life3d(ModeInfo * mi)
{
	life3dstruct *lp;

	if (life3ds == NULL)
		return;
	lp = &life3ds[MI_SCREEN(mi)];

	if (lp->painted) {
		MI_CLEARWINDOW(mi);
	}
}

void
change_life3d(ModeInfo * mi)
{
	int         npats;
	life3dstruct *lp;

	if (life3ds == NULL)
		return;
	lp = &life3ds[MI_SCREEN(mi)];

	lp->generation = 0;

	if (lp->eraserhead.next != NULL)
		End3D(lp);
	/*lp->tablesMade = 0; */

	MI_CLEARWINDOW(mi);

	lp->pattern++;
	lp->patterned_rule = codeToPatternedRule(lp->param);
	if ((unsigned) lp->patterned_rule < LIFE_RULES)
		npats = patterns_rules[lp->patterned_rule];
	else
		npats = 0;
	if (lp->pattern >= npats + 2) {
		lp->pattern = 0;
		if (allPatterns) {
			lp->patterned_rule++;
			if ((unsigned) lp->patterned_rule >= LIFE_RULES)
				lp->patterned_rule = 0;
			copyFromPatternedRule(&lp->param, lp->patterned_rule);
			printRule(lp->ruleString, lp->param, MI_IS_VERBOSE(mi));
		} else if (allGliders) {
			lp->patterned_rule++;
			if (lp->patterned_rule >= LIFE_GLIDERS)
				lp->patterned_rule = 0;
			copyFromPatternedRule(&lp->param, lp->patterned_rule);
			printRule(lp->ruleString, lp->param, MI_IS_VERBOSE(mi));
		}
	}
	if (!serial)
		lp->pattern = NRAND(npats + 2);
	if (lp->pattern >= npats) {
		if (!RandomSoup(mi, 30, 10)) {
			if (lp->eraserhead.next != NULL)
				End3D(lp);
			return;
		}
	} else {
		if (!GetPattern(mi, lp->patterned_rule, lp->pattern)) {
			if (lp->eraserhead.next != NULL)
				End3D(lp);
			return;
		}
	}

	DrawScreen(mi);
}

#endif /* MODE_life3d */
