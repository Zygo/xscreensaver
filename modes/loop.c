/* -*- Mode: C; tab-width: 4 -*- */
/* loop --- Chris Langton's self-producing loops */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)loop.c	5.01 2000/03/15 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by David Bagley.
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
 * 15-Mar-2001: Added some flaws, random blue wall spots, to liven it up.
 *              This mod seems to expose a bug where hexagons are erased
 *              for no apparent reason.
 * 01-Nov-2000: Allocation checks
 * 16-Jun-2000: Fully coded the hexagonal rules.  (Rules that end up in
 *              state zero were not bothered with since a calloc was used
 *              to set non-explicit rules to zero.  This allows the
 *              compile-time option RAND_RULES to work here (at least to
 *              generation 540).)
 *              Also added compile-time option DELAYDEBUGLOOP for debugging
 *              life form.  This also turns off the random initial direction
 *              of the loop.  Set DELAYDEBUGLOOP to be 10 and use with
 *              something like this:
 *       xlock -mode loop -neighbors 6 -size 5 -delay 1 -count 540 -nolock
 * 18-Oct-1998: Started creating a hexagon version.
 *              It proved not that difficult because I used Langton's Loop
 *              as a guide, hexagons have more neighbors so there is more
 *              freedom to program, and the loop will has six sides to
 *              store its genes (data).
 *              (Soon after this a triangular version with neighbors = 6
 *              was attempted but was unsuccessful).
 * 10-May-1997: Compatible with xscreensaver
 * 15-Nov-1995: Coded from Chris Langton's Self-Reproduction in Cellular
 *              Automata Physica 10D 135-144 1984, also used wire.c as a
 *              guide.
 */

/*-
  Grid     Number of Neighbors
  ----     ------------------
  Square   4
  Hexagon  6  (currently in development)
*/

/*-
 * From Steven Levy's Artificial Life
 * Chris Langton's cellular automata "loops" reproduce in the spirit of life.
 * Beginning from a single organism, the loops from a colony.  As the loops
 * on the outer fringes reproduce, the inner loops -- blocked by their
 * daughters -- can no longer produce offspring.  These dead progenitors
 * provide a base for future generations' expansion, much like the formation
 * of a coral reef.  This self-organizing behavior emerges spontaneously,
 * from the bottom up -- a key characteristic of artificial life.
 */

/*-
   Don't Panic  --  When the artificial life tries to leave its petri
   dish (ie. the screen) it will (usually) die...
   The loops are short of "real" life because a general purpose Turing
   machine is not contained in the loop.  This is a simplification of
   von Neumann and Codd's self-producing Turing machine.
   The data spinning around could be viewed as both its DNA and its internal
   clock.  The program can be initalized to have the loop spin both ways...
   but only one way around will work for a given rule.  An open question (at
   least to me): Is handedness a requirement for (artificial) life?  Here
   there is handedness at both the initial condition and the transition rule.
 */

#ifdef STANDALONE
#define PROGCLASS "loop"
#define HACK_INIT init_loop
#define HACK_DRAW draw_loop
#define loop_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*count: -5 \n" \
 "*cycles: 1600 \n" \
 "*size: -12 \n" \
 "*ncolors: 15 \n" \
 "*neighbors: 0 \n"
#define UNIFORM_COLORS
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */
#include "automata.h"

#ifdef MODE_loop

/*-
 * neighbors of 0 randomizes between 4 and 6.
 */
#define DEF_NEIGHBORS  "0"      /* choose random value */

static int  neighbors;

static XrmOptionDescRec opts[] =
{
	{(char *) "-neighbors", (char *) ".loop.neighbors", XrmoptionSepArg, (caddr_t) NULL}
};

static argtype vars[] =
{
	{(caddr_t *) & neighbors, (char *) "neighbors", (char *) "Neighbors", (char *) DEF_NEIGHBORS, t_Int}
};

static OptionStruct desc[] =
{
	{(char *) "-neighbors num", (char *) "squares 4 or hexagons 6"}
};

ModeSpecOpt loop_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};


#ifdef USE_MODULES
ModStruct   loop_description =
{"loop", "init_loop", "draw_loop", "release_loop",
 "refresh_loop", "init_loop", NULL, &loop_opts,
 100000, 5, 1600, -12, 64, 1.0, "",
 "Shows Langton's self-producing loops", 0, NULL};

#endif

#define LOOPBITS(n,w,h)\
  if ((lp->pixmaps[lp->init_bits]=\
  XCreatePixmapFromBitmapData(display,window,(char *)n,w,h,1,0,1))==None){\
  free_loop(display,lp); return;} else {lp->init_bits++;}

static int  local_neighbors = 0;

#if 0
/* Used to fast forward to troubled generations, mainly used for debugging.
   -delay 1 -count 170 -neighbors 6  # divisions starts
   540 first cell collision
   1111 mutant being born from 2 parents
   1156 mutant born 
 */
#define DELAYDEBUGLOOP 10
#endif

#define COLORS 8
#define REALCOLORS (COLORS-2)
#define MINLOOPS 1
#define REDRAWSTEP 2000		/* How many cells to draw per cycle */
#define ADAM_SIZE 8 /* MIN 5 */
#if 1
#define ADAM_LOOPX  (ADAM_SIZE+2)
#define ADAM_LOOPY  (ADAM_SIZE+2)
#else
#define ADAM_LOOPX 16
#define ADAM_LOOPY 10
#endif
#define MINGRIDSIZE (3*ADAM_LOOPX)
#if 0
/* TRIA stuff was an attempt to make a triangular lifeform on a
   hexagonal grid but I got bored.  You may need an additional 7th
   state for a coherent step by step process of cell separation and
   initial stem development.
 */
#define TRIA 1
#endif
#ifdef TRIA
#define HEX_ADAM_SIZE 3 /* MIN 3 */
#else
#define HEX_ADAM_SIZE 5 /* MIN 3 */
#endif
#if 1
#define HEX_ADAM_LOOPX (2*HEX_ADAM_SIZE+1)
#define HEX_ADAM_LOOPY (2*HEX_ADAM_SIZE+1)
#else
#define HEX_ADAM_LOOPX 3
#define HEX_ADAM_LOOPY 7
#endif
#define HEX_MINGRIDSIZE (6*HEX_ADAM_LOOPX)
#define MINSIZE ((MI_NPIXELS(mi)>=COLORS)?1:(2+(local_neighbors==6)))
#define NEIGHBORKINDS 2
#define ANGLES 360
#define MAXNEIGHBORS 6

/* Singly linked list */
typedef struct _CellList {
	XPoint      pt;
	struct _CellList *next;
} CellList;

typedef struct {
	int         init_bits;
	int         generation;
	int         xs, ys;
	int         xb, yb;
	int         nrows, ncols;
	int         bx, by, bnrows, bncols;
	int         mincol, minrow, maxcol, maxrow;
	int         width, height;
	int         redrawing, redrawpos;
	Bool        dead, clockwise;
	unsigned char *newcells, *oldcells;
	int         ncells[COLORS];
	CellList   *cellList[COLORS];
	unsigned long colors[COLORS];
	GC          stippledGC;
	Pixmap      pixmaps[COLORS];
	union {
		XPoint      hexagon[6];
	} shape;
} loopstruct;

static loopstruct *loops = NULL;

#define TRANSITION(TT,V) V=TT&7;TT>>=3
#define FINALTRANSITION(TT,V) V=TT&7
#define TABLE(R,T,L,B) (table[((B)<<9)|((L)<<6)|((T)<<3)|(R)])
#define HEX_TABLE(R,T,t,l,b,B) (table[((B)<<15)|((b)<<12)|((l)<<9)|((t)<<6)|((T)<<3)|(R)])

#if 0
/* Instead of setting "unused" state rules to zero it randomizes them.
   These rules take over when something unexpected happens... like when a
   cell hits a wall (the end of the screen).
 */
#define RAND_RULES
#endif

#ifdef RAND_RULES
#define TABLE_IN(C,R,T,L,B,I) (TABLE(R,T,L,B)&=~(7<<((C)*3)));\
(TABLE(R,T,L,B)|=((I)<<((C)*3)))
#define HEX_TABLE_IN(C,R,T,t,l,b,B,I) (HEX_TABLE(R,T,t,l,b,B)&=~(7<<((C)*3)));\
(HEX_TABLE(R,T,t,l,b,B)|=((I)<<((C)*3)))
#else
#define TABLE_IN(C,R,T,L,B,I) (TABLE(R,T,L,B)|=((I)<<((C)*3)))
#define HEX_TABLE_IN(C,R,T,t,l,b,B,I) (HEX_TABLE(R,T,t,l,b,B)|=((I)<<((C)*3)))
#endif
#define TABLE_OUT(C,R,T,L,B) ((TABLE(R,T,L,B)>>((C)*3))&7)
#define HEX_TABLE_OUT(C,R,T,t,l,b,B) ((HEX_TABLE(R,T,t,l,b,B)>>((C)*3))&7)

static unsigned int *table = NULL;	/* 8*8*8*8 = 2^12 = 2^3^4 = 4096 */
  /* 8*8*8*8*8*8 = 2^18 = 2^3^6 = 262144 = too big? */

static char plots[NEIGHBORKINDS] =
{
  4, 6 /* Neighborhoods */
};

static unsigned int transition_table[] =
{				/* Octal  CBLTR->I */
  /* CBLTRI   CBLTRI   CBLTRI   CBLTRI   CBLTRI */
    0000000, 0025271, 0113221, 0202422, 0301021,
    0000012, 0100011, 0122244, 0202452, 0301220,
    0000020, 0100061, 0122277, 0202520, 0302511,
    0000030, 0100077, 0122434, 0202552, 0401120,
    0000050, 0100111, 0122547, 0202622, 0401220,
    0000063, 0100121, 0123244, 0202722, 0401250,
    0000071, 0100211, 0123277, 0203122, 0402120,
    0000112, 0100244, 0124255, 0203216, 0402221,
    0000122, 0100277, 0124267, 0203226, 0402326,
    0000132, 0100511, 0125275, 0203422, 0402520,
    0000212, 0101011, 0200012, 0204222, 0403221,
    0000220, 0101111, 0200022, 0205122, 0500022,
    0000230, 0101244, 0200042, 0205212, 0500215,
    0000262, 0101277, 0200071, 0205222, 0500225,
    0000272, 0102026, 0200122, 0205521, 0500232,
    0000320, 0102121, 0200152, 0205725, 0500272,
    0000525, 0102211, 0200212, 0206222, 0500520,
    0000622, 0102244, 0200222, 0206722, 0502022,
    0000722, 0102263, 0200232, 0207122, 0502122,
    0001022, 0102277, 0200242, 0207222, 0502152,
    0001120, 0102327, 0200250, 0207422, 0502220,
    0002020, 0102424, 0200262, 0207722, 0502244,
    0002030, 0102626, 0200272, 0211222, 0502722,
    0002050, 0102644, 0200326, 0211261, 0512122,
    0002125, 0102677, 0200423, 0212222, 0512220,
    0002220, 0102710, 0200517, 0212242, 0512422,
    0002322, 0102727, 0200522, 0212262, 0512722,
    0005222, 0105427, 0200575, 0212272, 0600011,
    0012321, 0111121, 0200722, 0214222, 0600021,
    0012421, 0111221, 0201022, 0215222, 0602120,
    0012525, 0111244, 0201122, 0216222, 0612125,
    0012621, 0111251, 0201222, 0217222, 0612131,
    0012721, 0111261, 0201422, 0222272, 0612225,
    0012751, 0111277, 0201722, 0222442, 0700077,
    0014221, 0111522, 0202022, 0222462, 0701120,
    0014321, 0112121, 0202032, 0222762, 0701220,
    0014421, 0112221, 0202052, 0222772, 0701250,
    0014721, 0112244, 0202073, 0300013, 0702120,
    0016251, 0112251, 0202122, 0300022, 0702221,
    0017221, 0112277, 0202152, 0300041, 0702251,
    0017255, 0112321, 0202212, 0300076, 0702321,
    0017521, 0112424, 0202222, 0300123, 0702525,
    0017621, 0112621, 0202272, 0300421, 0702720,
    0017721, 0112727, 0202321, 0300622
};

static unsigned int hex_transition_table[] =
{				/* Octal CBbltTR->I */
  /* CBbltTRI   CBbltTRI   CBbltTRI   CBbltTRI   CBbltTRI */

#ifdef TRIA
    000000000, 000000020, 000000220, 000002220, 000022220,
    011122121, 011121221, 011122221, 011221221,
    011222221, 011112121, 011112221,
    020021122, 020002122, 020211222, 021111222,
    020221122, 020027122, 020020722, 020021022,
    001127221,
    011122727, 011227227, 010122121, 010222211,
    021117222, 020112272,
    070221220,
    001227221,
    010221121, 011721221, 011222277,
    020111222, 020221172,
    070211220,
    001217221,
    010212277, 010221221,
    020122112,
    070122220,
    001722221,
    010221271,
    020002022, 021122172,
    070121220,
    011122277, 011172121,
    010212177, 011212277,
    070112220,
    001772221,
    021221772,
    070121270, 070721220,
    000112721, 000272211,
    010022211, 012222277,
    020072272, 020227122, 020217222,
    010211121,
    020002727,
    070222220,
    001727721,
    020021072, 020070722,
    070002072, 070007022,
    001772721,
    070002022,
    000000070, 000000770, 000072220, 000000270,
    020110222, 020220272, 020220722,
    070007071, 070002072, 070007022,
    000000012, 000000122, 000000212, 001277721,
    020122072, 020202212,
    010002121,
    020001122, 020002112,
    020021722,
    020122022, 020027022, 020070122, 020020122,
    010227027,
    020101222,
    010227227, 010227277,
    021722172,
    001727221,
    010222277,
    020702272,
    070122020,
    000172721,
    010022277, 010202177, 010227127,

    001214221,
    010202244,
    020024122, 020020422,
    040122220,
    001422221,
    010221241, 010224224,
    021122142,
    040121220,
    001124221,
    010224274,
    020112242, 021422172,
    040221220,
    001224221, 001427221,
    010222244,
    020227042,
    040122020,
    000142721,
    010022244, 010202144, 010224124,
    040112220,
    001442221,
    021221442,
    040121240, 040421220,
    000242211, 000112421,
    020042242, 020214222, 020021422, 020220242, 020024022,
    011224224,
    020224122,
    020220422,
    012222244,
    020002424,
    040222220,
    001244421, 000000420, 000000440, 000000240, 000000040,
    020040121, 020021042,
    040004022, 040004042, 040002042,
    010021121,
    020011122, 020002112,
    001424421,
    020040422,
    001442421,
    040002022,
    001724221,
    010227247,
    020224072, 021417222,
    000172421,
    010021721,
    020017022,
    020120212,
    020271727,
    070207072, 070701220,
    000001222,
    020110122,
    001277221,
    001777721,
    020021222, 020202272, 020120222, 020221722,
    020027227,
    070070222,
    000007220,
    020101272, 020272172, 020721422, 020721722,
    020011222, 020202242,
#if 0
              {2,2,0,0,2,7,0},
             {2,0,2,0,2,0,2},
            {2,4,1,2,2,1,2},
           {2,1,2,1,2,1,2},
          {2,0,2,2,1,1,2},
         {2,7,1,1,1,1,2},
        {0,2,2,2,2,2,2},
              {2,2,0,0,7,7,0},
             {2,1,2,0,2,0,7},
            {2,0,1,2,2,1,2},
           {2,4,2,1,2,1,2},
          {2,1,2,2,1,1,2},
         {2,0,7,1,1,1,2},
        {0,2,2,2,2,2,2},
#endif
#else
    000000000, 000000020, 000000220, 000002220,
    011212121, 011212221, 011221221, 011222221,
    020002122, 020021122, 020211122,

    010221221, 010222121,
    020002022, 020021022, 020020122, 020112022,

    010202121,
    020102022, 020202112,

    000000012, 000000122, 000000212,
    010002121,
    020001122, 020002112, 020011122,


    001227221, 001272221, 001272721,
    012212277, 011222727, 011212727,
    020021722, 020027122, 020020722, 020027022,
    020211722, 020202172, 020120272,
    020271122, 020202172, 020207122, 020217122,
    020120272, 020210722, 020270722,
    070212220, 070221220, 070212120,


    012222277,
    020002727,
    070222220,

    001277721, 000000070, 000000270, 000000720, 000000770,
    020070122, 020021072,
    070002072, 070007022, 070007071,

    020070722,
    070002022,

    010227227, 010222727, 010202727,
    020172022, 020202712,

    001224221, 001242221, 001242421,
    012212244, 011222424, 011212424,
    020021422, 020024122, 020020422, 020024022,
    020211422, 020202142, 020120242,
    020241122, 020202142, 020204122, 020214122,
    020120242, 020210422, 020240422,
    040212220, 040221220, 040212120,


    012222244,
    020002424,
    040222220,

    001244421, 000000040, 000000240, 000000420, 000000440,
    020040122, 020021042,
    040002042,
    040004021, 040004042,

    020040422,
    040002022,

    010224224, 010222424, 010202424,
    020142022, 020202412,
    020011722, 020112072, 020172072, 020142072,



    000210225, 000022015, 000022522,
    011225521,
    020120525, 020020152, 020005122, 020214255, 020021152,
    020255242,
    050215222, 050225121,

    000225220, 001254222,
    010221250, 011221251, 011225221,
    020025122, 020152152, 020211252, 020214522, 020511125,
    050212241, 05221120,
    040521225,

    000000250, 000000520, 000150220, 000220520, 000222210,
    001224251,
    010022152, 010251221, 010522121, 011212151, 011221251,
    011215221,
    020000220, 020002152, 020020220, 020022152,
    020021422, 020022152, 020022522, 020025425, 020050422,
    020051022, 020051122, 020211122, 020211222, 020215222,
    020245122,
    050021125, 050021025, 050011125, 051242221,
    041225220,

    000220250, 000220520, 001227521, 001275221,
    011257227, 011522727,
    020002052, 020002752, 020021052, 020057125,
    050020722, 050027125,
    070215220,

    070212255,
    071225220,
    020275122,
    051272521,
    020055725,
    020021552,
    012252277,
    050002521,
    020005725,

    050011022,
    000000155,
    020050722,
    001227250,
    010512727,
    010002151,
    020027112,
    001227251,
    012227257,
    050002125,
    020517122,
    050002025,
    020050102,
    050002725,
    020570722,
    001252721,
    020007051,
    020102052,
    020271072,
    050001122,
    010002151,
    011227257,
    020051722,
    020057022,
    020050122,


    020051422,
    011224254,
    012224254,

    020054022,
    050002425,
    040252220,
    020002454,


    000000540,
    001254425,
    050004024,
    040004051,

    000000142,
    040001522,
    010002547,
    020045122,
    051221240,
    020002512,
    020021522,


    020020022,
    021125522,
    020521122,
    020025022,
    020025522,
    020020522,

    020202222,
    020212222,
    021212222,
    021222722,
    021222422,
    020002222,
    020021222,
    020022122,
    020212122,
    020027222,
    020024222,
    020212722,
    020212422,
    020202122,
    001222221,
    020002522,

    020017125,
    010022722,
    020212052,

    020205052,
    070221250,

    000000050, 000005220, 000002270, 070252220,
    000000450, 000007220,
    000220220, 000202220, 000022020, 000020220,

    000222040,
    000220440,
    000022040,
    000040220,

    000252220,
    050221120, 010221520,
    002222220,

    000070220, 000220720,
    000020520, 000070250, 000222070, 000027020,
    000022070, 000202270, 000024020, 000220420,
    000220270, 000220240, 000072020, 000042020,
    000002020, 000002070, 000020270, 000020250,
    000270270, 000007020, 000040270,

    /* Collision starts (gen 540), not sure to have rules to save it
       or depend on calloc to intialize remaining rules to 0 so that
       the mutant will be born
     */
    000050220,
#endif
};


/*-
Neighborhoods are read as follows (rotations are not listed):
    T
  L C R  ==>  I
    B

   t T
  l C R  ==>  I
   b B
 */

static unsigned char self_reproducing_loop[ADAM_LOOPY][ADAM_LOOPX] =
{
/* 10x10 */
	{0, 2, 2, 2, 2, 2, 2, 2, 2, 0},
	{2, 4, 0, 1, 4, 0, 1, 1, 1, 2},
	{2, 1, 2, 2, 2, 2, 2, 2, 1, 2},
	{2, 0, 2, 0, 0, 0, 0, 2, 1, 2},
	{2, 7, 2, 0, 0, 0, 0, 2, 7, 2},
	{2, 1, 2, 0, 0, 0, 0, 2, 0, 2},
	{2, 0, 2, 0, 0, 0, 0, 2, 1, 2},
	{2, 7, 2, 2, 2, 2, 2, 2, 7, 2},
	{2, 1, 0, 6, 1, 0, 7, 1, 0, 2},
	{0, 2, 2, 2, 2, 2, 2, 2, 2, 0}
};

static unsigned char hex_self_reproducing_loop[HEX_ADAM_LOOPY][HEX_ADAM_LOOPX] =
{
#if 0
/* Experimental TRIA5:7x7 */
	      {2,2,0,0,0,0,0},
	     {2,1,2,0,2,2,0},
	    {2,0,4,2,2,0,2},
	   {2,7,2,0,2,0,2},
	  {2,1,2,2,1,1,2},
	 {2,0,7,1,0,7,2},
	{0,2,2,2,2,2,2},
  /* Stem cells, only "5" will fully reproduce itself */
/* 3:12x7 */
	      {2,2,2,2,0,0,0,0,0,0,0,0},
	     {2,1,1,1,2,0,0,0,0,0,0,0},
	    {2,1,2,2,1,2,2,2,2,2,2,0},
	   {2,1,2,0,2,7,1,1,1,1,1,2},
	  {0,2,1,2,2,0,2,2,2,2,2,2},
	 {0,0,2,0,4,1,2,0,0,0,0,0},
	{0,0,0,2,2,2,2,0,0,0,0,0}
/* 4:14x9 */
	        {2,2,2,2,2,0,0,0,0,0,0,0,0,0},
	       {2,1,1,1,1,2,0,0,0,0,0,0,0,0},
	      {2,1,2,2,2,1,2,0,0,0,0,0,0,0},
	     {2,1,2,0,0,2,1,2,2,2,2,2,2,0},
	    {2,1,2,0,0,0,2,7,1,1,1,1,1,2},
	   {0,2,1,2,0,0,2,0,2,2,2,2,2,2},
	  {0,0,2,0,2,2,2,1,2,0,0,0,0,0},
	 {0,0,0,2,4,1,0,7,2,0,0,0,0,0},
	{0,0,0,0,2,2,2,2,2,0,0,0,0,0}
/* 5:16x11 */
	          {2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0},
	         {2,1,1,1,1,1,2,0,0,0,0,0,0,0,0,0},
	        {2,1,2,2,2,2,1,2,0,0,0,0,0,0,0,0},
	       {2,1,2,0,0,0,2,1,2,0,0,0,0,0,0,0},
	      {2,1,2,0,0,0,0,2,1,2,2,2,2,2,2,0},
	     {2,1,2,0,0,0,0,0,2,7,1,1,1,1,1,2},
	    {0,2,1,2,0,0,0,0,2,0,2,2,2,2,2,2},
	   {0,0,2,0,2,0,0,0,2,1,2,0,0,0,0,0},
	  {0,0,0,2,4,2,2,2,2,7,2,0,0,0,0,0},
	 {0,0,0,0,2,1,0,7,1,0,2,0,0,0,0,0},
	{0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0}
/* test:3x7  (0,4) is blank  ... very strange.
          init_adam seems ok something after that I guess */
	             {2,2,0},
	            {2,0,2},
	           {0,2,2},
	          {0,0,0},
	         {2,2,0},
	        {2,1,2},
	       {0,2,2},
#else /* this might be better for hexagons, spacewise efficient... */
#ifdef TRIA
/* Experimental TRIA5:7x7 */
	      {2,2,0,0,2,2,0},
	     {2,4,2,0,2,7,2},
	    {2,1,0,2,2,0,2},
	   {2,0,2,1,2,1,2},
	  {2,7,2,2,7,7,2},
	 {2,1,0,7,1,0,2},
	{0,2,2,2,2,2,2},
#else
/* 5:11x11 */
	          {2,2,2,2,2,2,0,0,0,0,0},
	         {2,1,1,7,0,1,2,0,0,0,0},
	        {2,1,2,2,2,2,7,2,0,0,0},
	       {2,1,2,0,0,0,2,0,2,0,0},
	      {2,1,2,0,0,0,0,2,1,2,0},
	     {2,1,2,0,0,0,0,0,2,7,2},
	    {0,2,1,2,0,0,0,0,2,0,2},
	   {0,0,2,1,2,0,0,0,2,1,2},
	  {0,0,0,2,1,2,2,2,2,4,2},
	 {0,0,0,0,2,1,1,1,1,5,2},
	{0,0,0,0,0,2,2,2,2,2,2}
#endif
#endif
};

static void
position_of_neighbor(int dir, int *pcol, int *prow)
{
	int         col = *pcol, row = *prow;

	/* NO WRAPING */

	if (local_neighbors == 6) {
		switch (dir) {
			case 0:
				col++;
				break;
			case 60:
				col += (row & 1);
				row--;
				break;
			case 120:
				col -= !(row & 1);
				row--;
				break;
			case 180:
				col--;
				break;
			case 240:
				col -= !(row & 1);
				row++;
				break;
			case 300:
				col += (row & 1);
				row++;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	} else {
		switch (dir) {
			case 0:
				col++;
				break;
			case 90:
				row--;
				break;
			case 180:
				col--;
				break;
			case 270:
				row++;
				break;
			default:
				(void) fprintf(stderr, "wrong direction %d\n", dir);
		}
	}
	*pcol = col;
	*prow = row;
}

static      Bool
withinBounds(loopstruct * lp, int col, int row)
{
	return (row >= 1 && row < lp->bnrows - 1 &&
		col >= 1 && col < lp->bncols - 1 - (local_neighbors == 6 && (row % 2)));
}

static void
fillcell(ModeInfo * mi, GC gc, int col, int row)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];

	if (local_neighbors == 6) {
		int         ccol = 2 * col + !(row & 1), crow = 2 * row;

		lp->shape.hexagon[0].x = lp->xb + ccol * lp->xs;
		lp->shape.hexagon[0].y = lp->yb + crow * lp->ys;
		if (lp->xs == 1 && lp->ys == 1)
			XDrawPoint(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
				lp->shape.hexagon[0].x, lp->shape.hexagon[0].y);
		else
			XFillPolygon(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
				lp->shape.hexagon, 6, Convex, CoordModePrevious);
	} else {
		XFillRectangle(MI_DISPLAY(mi), MI_WINDOW(mi), gc,
			lp->xb + lp->xs * col, lp->yb + lp->ys * row,
			lp->xs - (lp->xs > 3), lp->ys - (lp->ys > 3));
	}
}

static void
drawcell(ModeInfo * mi, int col, int row, int state)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	XGCValues   gcv;
	GC          gc;

	if (MI_NPIXELS(mi) >= COLORS) {
		gc = MI_GC(mi);
		XSetForeground(MI_DISPLAY(mi), gc, lp->colors[state]);
	} else {
		gcv.stipple = lp->pixmaps[state];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(MI_DISPLAY(mi), lp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = lp->stippledGC;
	}
	fillcell(mi, gc, col, row);
}

#ifdef DEBUG
static void
print_state(ModeInfo * mi, int state)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	CellList   *locallist = lp->cellList[state];
	int         i = 0;

	(void) printf("state %d\n", state);
	while (locallist) {
		(void) printf("%d x %d, y %d\n", i,
			      locallist->pt.x, locallist->pt.y);
		locallist = locallist->next;
		i++;
	}
}

#endif

static void
free_state(loopstruct * lp, int state)
{
	CellList   *current;

	while (lp->cellList[state]) {
		current = lp->cellList[state];
		lp->cellList[state] = lp->cellList[state]->next;
		(void) free((void *) current);
	}
	lp->ncells[state] = 0;
}

static void
free_list(loopstruct * lp)
{
	int         state;

	for (state = 0; state < COLORS; state++)
		free_state(lp, state);
}

static void
free_loop(Display *display, loopstruct * lp)
{
	int         shade;

	for (shade = 0; shade < lp->init_bits; shade++)
		if (lp->pixmaps[shade] != None) {
			XFreePixmap(display, lp->pixmaps[shade]);
			lp->pixmaps[shade] = None;
		}
	if (lp->stippledGC != None) {
		XFreeGC(display, lp->stippledGC);
		lp->stippledGC = None;
	}
	if (lp->oldcells != NULL) {
		(void) free((void *) lp->oldcells);
		lp->oldcells = NULL;
	}
	if (lp->newcells != NULL) {
		(void) free((void *) lp->newcells);
		lp->newcells = NULL;
	}
	free_list(lp);
}

static Bool
addtolist(ModeInfo * mi, int col, int row, unsigned char state)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	CellList   *current = lp->cellList[state];

	if ((lp->cellList[state] = (CellList *) malloc(sizeof (CellList))) ==
			NULL) {
		lp->cellList[state] = current;
		free_loop(MI_DISPLAY(mi), lp);
		return False;
	}
	lp->cellList[state]->pt.x = col;
	lp->cellList[state]->pt.y = row;
	lp->cellList[state]->next = current;
	lp->ncells[state]++;
	return True;
}

static Bool
draw_state(ModeInfo * mi, int state)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	Display    *display = MI_DISPLAY(mi);
	GC          gc;
	XGCValues   gcv;
	CellList   *current = lp->cellList[state];

	if (MI_NPIXELS(mi) >= COLORS) {
		gc = MI_GC(mi);
		XSetForeground(display, gc, lp->colors[state]);
	} else {
		gcv.stipple = lp->pixmaps[state];
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		XChangeGC(display, lp->stippledGC,
			  GCStipple | GCForeground | GCBackground, &gcv);
		gc = lp->stippledGC;
	}

	if (local_neighbors == 6) {       /* Draw right away, slow */
		while (current) {
			int	 col, row, ccol, crow;

			col = current->pt.x;
			row = current->pt.y;
			ccol = 2 * col + !(row & 1), crow = 2 * row;
			lp->shape.hexagon[0].x = lp->xb + ccol * lp->xs;
			lp->shape.hexagon[0].y = lp->yb + crow * lp->ys;
			if (lp->xs == 1 && lp->ys == 1)
				XDrawPoint(display, MI_WINDOW(mi), gc,
					lp->shape.hexagon[0].x, lp->shape.hexagon[0].y);
			else
				XFillPolygon(display, MI_WINDOW(mi), gc,
				  	lp->shape.hexagon, 6, Convex, CoordModePrevious);
			current = current->next;
		}
	} else {
		/* Take advantage of XFillRectangles */
		XRectangle *rects;
		int         nrects = 0;

		/* Create Rectangle list from part of the cellList */
		if ((rects = (XRectangle *) malloc(lp->ncells[state] *
				sizeof (XRectangle))) == NULL) {
			return False;
		}

		while (current) {
			rects[nrects].x = lp->xb + current->pt.x * lp->xs;
			rects[nrects].y = lp->yb + current->pt.y * lp->ys;
			rects[nrects].width = lp->xs - (lp->xs > 3);
			rects[nrects].height = lp->ys - (lp->ys > 3);
			current = current->next;
			nrects++;
		}
		/* Finally get to draw */
		XFillRectangles(display, MI_WINDOW(mi), gc, rects, nrects);
		/* Free up rects list and the appropriate part of the cellList */
		(void) free((void *) rects);
	}
	free_state(lp, state);
	XFlush(display);
	return True;
}

static Bool
init_table()
{
	if (table == NULL) {
		int mult = 1;
		unsigned int tt, c, n[MAXNEIGHBORS], i;
		int         j, k;
		int  size_transition_table = sizeof (transition_table) /
			sizeof (unsigned int);
		int  size_hex_transition_table = sizeof (hex_transition_table) /
			sizeof (unsigned int);

		for (j = 0; j < local_neighbors; j++)
			mult *= 8;

		if ((table = (unsigned int *) calloc(mult, sizeof (unsigned int))) == NULL) {
			return False;
		}


#ifdef RAND_RULES
		/* Here I was interested to see what happens when it hits a wall....
		   Rules not normally used take over... takes too much time though */
		/* Each state = 3 bits */
		if (MAXRAND < 16777216.0) {
			for (j = 0; j < mult; j++) {
				table[j] = (unsigned int) ((NRAND(4096) << 12) & NRAND(4096));
			}
		} else {
			for  (j = 0; j < mult; j++) {
				table[j] = (unsigned int) (NRAND(16777216));
			}
		}
#endif
		if (local_neighbors == 6) {
			for (j = 0; j < size_hex_transition_table; j++) {
				tt = hex_transition_table[j];
				TRANSITION(tt, i);
				for (k = 0; k < local_neighbors; k++) {
					TRANSITION(tt, n[k]);
				}
				FINALTRANSITION(tt, c);
				HEX_TABLE_IN(c, n[0], n[1], n[2], n[3], n[4], n[5], i);
				HEX_TABLE_IN(c, n[1], n[2], n[3], n[4], n[5], n[0], i);
				HEX_TABLE_IN(c, n[2], n[3], n[4], n[5], n[0], n[1], i);
				HEX_TABLE_IN(c, n[3], n[4], n[5], n[0], n[1], n[2], i);
				HEX_TABLE_IN(c, n[4], n[5], n[0], n[1], n[2], n[3], i);
				HEX_TABLE_IN(c, n[5], n[0], n[1], n[2], n[3], n[4], i);
			}
		} else {
			for (j = 0; j < size_transition_table; j++) {
				tt = transition_table[j];
				TRANSITION(tt, i);
				for (k = 0; k < local_neighbors; k++) {
					TRANSITION(tt, n[k]);
				}
				FINALTRANSITION(tt, c);
				TABLE_IN(c, n[0], n[1], n[2], n[3], i);
				TABLE_IN(c, n[1], n[2], n[3], n[0], i);
				TABLE_IN(c, n[2], n[3], n[0], n[1], i);
				TABLE_IN(c, n[3], n[0], n[1], n[2], i);
			}
		}
	}
	return True;
}

static void
init_flaw(ModeInfo * mi)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	int a, b;

#define	BLUE 2
	if (lp->bncols <= 3 || lp->bnrows <= 3)
		return;
	a = NRAND(lp->bncols - 3);
	b = NRAND(lp->bnrows - 3);
	if (lp->mincol > a)
		lp->mincol = a;
	if (lp->minrow > b)
		lp->minrow = b;
	if (lp->maxcol < a + 2)
		lp->maxcol = a + 2;
	if (lp->maxrow < b + 2)
		lp->maxrow = b + 2;

	if (local_neighbors == 6) {
		lp->newcells[b * lp->bncols + a + !(b % 2) ] = BLUE;
		lp->newcells[b * lp->bncols + a + 1 + !(b % 2)] = BLUE;
		lp->newcells[(b + 1) * lp->bncols + a] = BLUE;
		lp->newcells[(b + 1) * lp->bncols + a + 2] = BLUE;
		lp->newcells[(b + 2) * lp->bncols + a + !(b % 2)] = BLUE;
		lp->newcells[(b + 2) * lp->bncols + a + 1 + !(b % 2)] = BLUE;
	} else {
		int orient = NRAND(4);
		lp->newcells[lp->bncols * (b + 1) + a + 1] = BLUE;
		if (orient == 0 || orient == 1) {
			lp->newcells[lp->bncols * b + a + 1] = BLUE;
		}
		if (orient == 1 || orient == 2) {
			lp->newcells[lp->bncols * (b + 1) + a + 2] = BLUE;
		}
		if (orient == 2 || orient == 3) {
			lp->newcells[lp->bncols * (b + 2) + a + 1] = BLUE;
		}
		if (orient == 3 || orient == 0) {
			lp->newcells[lp->bncols * (b + 1) + a] = BLUE;
		}
	}
}

static void
init_adam(ModeInfo * mi)
{
	loopstruct *lp = &loops[MI_SCREEN(mi)];
	XPoint      start, dirx, diry;
	int         i, j, dir;

#ifdef DELAYDEBUGLOOP
	lp->clockwise = 0;
	if (!MI_COUNT(mi)) /* Probably doing testing so do not confuse */
#endif
	lp->clockwise = (Bool) (LRAND() & 1);
#ifdef DELAYDEBUGLOOP
	dir = 0;
	if (!MI_COUNT(mi)) /* Probably doing testing so do not confuse */
#endif
	dir = NRAND(local_neighbors);
	if (local_neighbors == 6) {
		int k;

		switch (dir) {
			case 0:
				start.x = (lp->bncols - HEX_ADAM_LOOPX / 2) / 2;
				start.y = (lp->bnrows - HEX_ADAM_LOOPY) / 2;
				if (lp->mincol > start.x - 2)
					lp->mincol = start.x - 2;
				if (lp->minrow > start.y - 1)
					lp->minrow = start.y - 1;
				if (lp->maxcol < start.x + HEX_ADAM_LOOPX + 1)
					lp->maxcol = start.x + HEX_ADAM_LOOPX + 1;
				if (lp->maxrow < start.y + HEX_ADAM_LOOPY + 1)
					lp->maxrow = start.y + HEX_ADAM_LOOPY + 1;
				for (j = 0; j < HEX_ADAM_LOOPY; j++) {
					for (i = 0; i < HEX_ADAM_LOOPX; i++) {
						k = (((lp->bnrows / 2 + HEX_ADAM_LOOPY / 2) % 2) ? -j / 2 : -(j + 1) / 2);
						lp->newcells[(start.y + j) * lp->bncols + start.x + i + k] =
		 					(lp->clockwise) ?
					hex_self_reproducing_loop[i][j] :
					hex_self_reproducing_loop[j][i];
					}
				}
				break;
			case 1:
				start.x = (lp->bncols - (HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2) / 2;
				start.y = (lp->bnrows - HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2;
				if (lp->mincol > start.x - 1)
					lp->mincol = start.x - 1;
				if (lp->minrow > start.y - HEX_ADAM_LOOPX)
					lp->minrow = start.y - HEX_ADAM_LOOPX;
				if (lp->maxcol < start.x + (HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2 + 1)
					lp->maxcol = start.x + (HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2 + 1;
				if (lp->maxrow < start.y + HEX_ADAM_LOOPY + 1)
					lp->maxrow = start.y + HEX_ADAM_LOOPY + 1;
				for (j = 0; j < HEX_ADAM_LOOPY; j++) {
					for (i = 0; i < HEX_ADAM_LOOPX; i++) {
						k = (((lp->bnrows / 2 + (HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2) % 2)
              ? -(i + j + 1) / 2 : -(i + j) / 2);
						lp->newcells[(start.y + j - i) * lp->bncols + start.x + i + j + k] =
		 					(lp->clockwise) ?
		 					hex_self_reproducing_loop[i][j] :
		 					hex_self_reproducing_loop[j][i];
					}
				}
				break;
			case 2:
				start.x = (lp->bncols - HEX_ADAM_LOOPY / 2) / 2;
				start.y = (lp->bnrows - HEX_ADAM_LOOPX) / 2;
				if (lp->mincol > start.x - 2)
					lp->mincol = start.x - 2;
				if (lp->minrow > start.y - 1)
					lp->minrow = start.y - 1;
				if (lp->maxcol < start.x + HEX_ADAM_LOOPX + 1)
					lp->maxcol = start.x + HEX_ADAM_LOOPX + 1;
				if (lp->maxrow < start.y + HEX_ADAM_LOOPY + 1)
					lp->maxrow = start.y + HEX_ADAM_LOOPY + 1;
				for (j = 0; j < HEX_ADAM_LOOPX; j++) {
					for (i = 0; i < HEX_ADAM_LOOPY; i++) {
						k = (((lp->bnrows / 2 + HEX_ADAM_LOOPX / 2) % 2) ? -(HEX_ADAM_LOOPX - j - 1) / 2 : -(HEX_ADAM_LOOPX - j) / 2);
						lp->newcells[(start.y + j) * lp->bncols + start.x + i + k] =
		 					(lp->clockwise) ?
		 					hex_self_reproducing_loop[j][HEX_ADAM_LOOPX - i - 1] :
		 					hex_self_reproducing_loop[i][HEX_ADAM_LOOPY - j - 1];
					}
				}
				break;
			case 3:
				start.x = (lp->bncols - HEX_ADAM_LOOPX / 2) / 2;
				start.y = (lp->bnrows - HEX_ADAM_LOOPY) / 2;
				if (lp->mincol > start.x - 1)
					lp->mincol = start.x - 1;
				if (lp->minrow > start.y - 1)
					lp->minrow = start.y - 1;
				if (lp->maxcol < start.x + HEX_ADAM_LOOPX + 1)
					lp->maxcol = start.x + HEX_ADAM_LOOPX + 1;
				if (lp->maxrow < start.y + HEX_ADAM_LOOPY + 1)
					lp->maxrow = start.y + HEX_ADAM_LOOPY + 1;
				for (j = 0; j < HEX_ADAM_LOOPY; j++) {
					for (i = 0; i < HEX_ADAM_LOOPX; i++) {
						k = (((lp->bnrows / 2 + HEX_ADAM_LOOPY / 2) % 2) ? -j / 2 : -(j + 1) / 2);
						lp->newcells[(start.y + j) * lp->bncols + start.x + i + k] =
		 					(lp->clockwise) ?
		 					hex_self_reproducing_loop[HEX_ADAM_LOOPX - i - 1][HEX_ADAM_LOOPY - j - 1] :
		 					hex_self_reproducing_loop[HEX_ADAM_LOOPY - j - 1][HEX_ADAM_LOOPX - i - 1];
					}
				}
				break;
			case 4:
				start.x = (lp->bncols - (HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2) / 2;
				start.y = (lp->bnrows - HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2;
				if (lp->mincol > start.x - 1)
					lp->mincol = start.x - 1;
				if (lp->minrow > start.y - HEX_ADAM_LOOPX)
					lp->minrow = start.y - HEX_ADAM_LOOPX;
				if (lp->maxcol < start.x + (HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2 + 1)
					lp->maxcol = start.x + (HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2 + 1;
				if (lp->maxrow < start.y + HEX_ADAM_LOOPY + 1)
					lp->maxrow = start.y + HEX_ADAM_LOOPY + 1;
				for (j = 0; j < HEX_ADAM_LOOPY; j++) {
					for (i = 0; i < HEX_ADAM_LOOPX; i++) {
						k = (((lp->bnrows / 2 + (HEX_ADAM_LOOPX + HEX_ADAM_LOOPY) / 2) % 2)
              ? -(i + j + 1) / 2 : -(i + j) / 2);
						lp->newcells[(start.y + j - i) * lp->bncols + start.x + i + j + k] =
		 					(lp->clockwise) ?
		 					hex_self_reproducing_loop[HEX_ADAM_LOOPX - i - 1][HEX_ADAM_LOOPY - j - 1] :
		 					hex_self_reproducing_loop[HEX_ADAM_LOOPY - j - 1][HEX_ADAM_LOOPX - i - 1];
					}
				}
				break;
			case 5:
				start.x = (lp->bncols - HEX_ADAM_LOOPY / 2) / 2;
				start.y = (lp->bnrows - HEX_ADAM_LOOPX) / 2;
				if (lp->mincol > start.x - 2)
					lp->mincol = start.x - 2;
				if (lp->minrow > start.y - 1)
					lp->minrow = start.y - 1;
				if (lp->maxcol < start.x + HEX_ADAM_LOOPY + 1)
					lp->maxcol = start.x + HEX_ADAM_LOOPY + 1;
				if (lp->maxrow < start.y + HEX_ADAM_LOOPX + 1)
					lp->maxrow = start.y + HEX_ADAM_LOOPX + 1;
				for (j = 0; j < HEX_ADAM_LOOPX; j++) {
					for (i = 0; i < HEX_ADAM_LOOPY; i++) {
						k = (((lp->bnrows / 2 + HEX_ADAM_LOOPX / 2) % 2) ? -(HEX_ADAM_LOOPX - j - 1) / 2 : -(HEX_ADAM_LOOPX - j) / 2);
						lp->newcells[(start.y + j) * lp->bncols + start.x + i + k] =
		 					(lp->clockwise) ?
		 					hex_self_reproducing_loop[HEX_ADAM_LOOPY - j - 1][i] :
		 					hex_self_reproducing_loop[HEX_ADAM_LOOPX - i - 1][j];
					}
				}
				break;
		}
#if DEBUGTEST
				/* (void) printf ("s %d  s %d \n", start.x, start.y); */
		(void) printf ("%d %d %d %d %d\n",
     start.x + i + ((lp->bnrows / 2 % 2) ? -j / 2 : -(j + 1) / 2) - lp->bx,
		 start.y + j - lp->by, i, j, hex_self_reproducing_loop[j][i]);
		/* Draw right away */
		drawcell(mi, start.x + i + ((lp->bnrows / 2 % 2) ? -j / 2 : -(j + 1) / 2) - lp->bx,
		 start.y + j - lp->by,
		 hex_self_reproducing_loop[j][i]);
#endif
  } else {
		switch (dir) {
			case 0:
				start.x = (lp->bncols - ADAM_LOOPX) / 2;
				start.y = (lp->bnrows - ADAM_LOOPY) / 2;
				dirx.x = 1, dirx.y = 0;
				diry.x = 0, diry.y = 1;
				if (lp->mincol > start.x)
					lp->mincol = start.x;
				if (lp->minrow > start.y)
					lp->minrow = start.y;
				if (lp->maxcol < start.x + ADAM_LOOPX)
					lp->maxcol = start.x + ADAM_LOOPX;
				if (lp->maxrow < start.y + ADAM_LOOPY)
					lp->maxrow = start.y + ADAM_LOOPY;
				break;
			case 1:
				start.x = (lp->bncols + ADAM_LOOPY) / 2;
				start.y = (lp->bnrows - ADAM_LOOPX) / 2;
				dirx.x = 0, dirx.y = 1;
				diry.x = -1, diry.y = 0;
				if (lp->mincol > start.x - ADAM_LOOPY)
					lp->mincol = start.x - ADAM_LOOPY;
				if (lp->minrow > start.y)
					lp->minrow = start.y;
				if (lp->maxcol < start.x)
					lp->maxcol = start.x;
				if (lp->maxrow < start.y + ADAM_LOOPX)
					lp->maxrow = start.y + ADAM_LOOPX;
				break;
			case 2:
				start.x = (lp->bncols + ADAM_LOOPX) / 2;
				start.y = (lp->bnrows + ADAM_LOOPY) / 2;
				dirx.x = -1, dirx.y = 0;
				diry.x = 0, diry.y = -1;
				if (lp->mincol > start.x - ADAM_LOOPX)
					lp->mincol = start.x - ADAM_LOOPX;
				if (lp->minrow > start.y - ADAM_LOOPY)
					lp->minrow = start.y - ADAM_LOOPY;
				if (lp->maxcol < start.x)
					lp->maxcol = start.x;
				if (lp->maxrow < start.y)
					lp->maxrow = start.y;
				break;
			case 3:
				start.x = (lp->bncols - ADAM_LOOPY) / 2;
				start.y = (lp->bnrows + ADAM_LOOPX) / 2;
				dirx.x = 0, dirx.y = -1;
				diry.x = 1, diry.y = 0;
				if (lp->mincol > start.x)
					lp->mincol = start.x;
				if (lp->minrow > start.y - ADAM_LOOPX)
					lp->minrow = start.y - ADAM_LOOPX;
				if (lp->maxcol < start.x + ADAM_LOOPX)
					lp->maxcol = start.x + ADAM_LOOPX;
				if (lp->maxrow < start.y)
					lp->maxrow = start.y;
				break;
		}
		for (j = 0; j < ADAM_LOOPY; j++)
			for (i = 0; i < ADAM_LOOPX; i++)
			  lp->newcells[lp->bncols * (start.y + dirx.y * i + diry.y * j) +
			    start.x + dirx.x * i + diry.x * j] =
			    (lp->clockwise) ?
			      self_reproducing_loop[j][ADAM_LOOPX - i - 1] :
			      self_reproducing_loop[j][i];
#if DEBUG
		/* Draw right away */
		drawcell(mi, start.x + dirx.x * i + diry.x * j - lp->bx,
		 start.y + dirx.y * i + diry.y * j - lp->by,
		 (lp->clockwise) ? self_reproducing_loop[j][ADAM_LOOPX - i - i] : self_reproducing_loop[j][i]);
#endif
	}
}


static void
do_gen(loopstruct * lp)
{
	int         i, j, k;
	unsigned char *z;
	unsigned int n[MAXNEIGHBORS];
	unsigned int c;

#define LOC(X, Y) (*(lp->oldcells + (X) + ((Y) * lp->bncols)))

	for (j = lp->minrow; j <= lp->maxrow; j++) {
		for (i = lp->mincol; i <= lp->maxcol; i++) {
			z = lp->newcells + i + j * lp->bncols;
			c = LOC(i, j);
			for (k = 0; k < local_neighbors; k++) {
				int         newi = i, newj = j;

				position_of_neighbor(k * ANGLES / local_neighbors, &newi, &newj);
				n[k] = 0;
		    if (withinBounds(lp, newi, newj)) {
					n[k] = LOC(newi, newj);
				}
			}
			if (local_neighbors == 6) {
				*z = (lp->clockwise) ?
					HEX_TABLE_OUT(c, n[5], n[4], n[3], n[2], n[1], n[0]) :
					HEX_TABLE_OUT(c, n[0], n[1], n[2], n[3], n[4], n[5]);
			} else {
				*z = (lp->clockwise) ?
					TABLE_OUT(c, n[3], n[2], n[1], n[0]) :
					TABLE_OUT(c, n[0], n[1], n[2], n[3]);
			}
		}
	}
}

void
release_loop(ModeInfo * mi)
{
	if (loops != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_loop(MI_DISPLAY(mi), &loops[screen]);
		(void) free((void *) loops);
		loops = NULL;
	}
	if (table != NULL) {
		(void) free((void *) table);
		table = NULL;
	}
}

void
init_loop(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         i, size = MI_SIZE(mi);
	loopstruct *lp;
	XGCValues   gcv;

	if (loops == NULL) {
		if ((loops = (loopstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (loopstruct))) == NULL)
			return;
	}
	lp = &loops[MI_SCREEN(mi)];

	lp->redrawing = 0;

	if ((MI_NPIXELS(mi) < COLORS) && (lp->init_bits == 0)) {
		if (lp->stippledGC == None) {
			gcv.fill_style = FillOpaqueStippled;
			if ((lp->stippledGC = XCreateGC(display, window,
				 GCFillStyle, &gcv)) == None) {
				free_loop(display, lp);
				return;
			}
		}
		LOOPBITS(stipples[0], STIPPLESIZE, STIPPLESIZE);
		LOOPBITS(stipples[2], STIPPLESIZE, STIPPLESIZE);
		LOOPBITS(stipples[3], STIPPLESIZE, STIPPLESIZE);
		LOOPBITS(stipples[4], STIPPLESIZE, STIPPLESIZE);
		LOOPBITS(stipples[6], STIPPLESIZE, STIPPLESIZE);
		LOOPBITS(stipples[7], STIPPLESIZE, STIPPLESIZE);
		LOOPBITS(stipples[8], STIPPLESIZE, STIPPLESIZE);
		LOOPBITS(stipples[10], STIPPLESIZE, STIPPLESIZE);
	}
	if (MI_NPIXELS(mi) >= COLORS) {
		/* Maybe these colors should be randomized */
		lp->colors[0] = MI_BLACK_PIXEL(mi);
		lp->colors[1] = MI_PIXEL(mi, 0);	/* RED */
		lp->colors[5] = MI_PIXEL(mi, MI_NPIXELS(mi) / REALCOLORS);	/* YELLOW */
		lp->colors[4] = MI_PIXEL(mi, 2 * MI_NPIXELS(mi) / REALCOLORS);	/* GREEN */
		lp->colors[6] = MI_PIXEL(mi, 3 * MI_NPIXELS(mi) / REALCOLORS);	/* CYAN */
		lp->colors[2] = MI_PIXEL(mi, 4 * MI_NPIXELS(mi) / REALCOLORS);	/* BLUE */
		lp->colors[3] = MI_PIXEL(mi, 5 * MI_NPIXELS(mi) / REALCOLORS);	/* MAGENTA */
		lp->colors[7] = MI_WHITE_PIXEL(mi);
	}
	free_list(lp);
	lp->generation = 0;
	lp->width = MI_WIDTH(mi);
	lp->height = MI_HEIGHT(mi);

	if (!local_neighbors) {
		for (i = 0; i < NEIGHBORKINDS; i++) {
			if (neighbors == plots[i]) {
				local_neighbors = neighbors;
				break;
			}
			if (i == NEIGHBORKINDS - 1) {
#if 1
				local_neighbors = plots[NRAND(NEIGHBORKINDS)];
#else
				local_neighbors = 4;
#endif
				break;
			}
		}
	}


  if (local_neighbors == 6) {
    int         nccols, ncrows;

    if (lp->width < 4)
      lp->width = 4;
    if (lp->height < 4)
      lp->height = 4;
    if (size < -MINSIZE) {
      lp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(lp->width, lp->height) /
              HEX_MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
    } else if (size < MINSIZE) {
      if (!size)
        lp->ys = MAX(MINSIZE, MIN(lp->width, lp->height) / HEX_MINGRIDSIZE);
      else
        lp->ys = MINSIZE;
    } else
      lp->ys = MIN(size, MAX(MINSIZE, MIN(lp->width, lp->height) /
                 HEX_MINGRIDSIZE));
    lp->xs = lp->ys;
    nccols = MAX(lp->width / lp->xs - 2, HEX_MINGRIDSIZE);
    ncrows = MAX(lp->height / lp->ys - 1, HEX_MINGRIDSIZE);
    lp->ncols = nccols / 2;
    lp->nrows = ncrows / 2;
    lp->nrows -= !(lp->nrows & 1);  /* Must be odd */
    lp->xb = (lp->width - lp->xs * nccols) / 2 + lp->xs;
    lp->yb = (lp->height - lp->ys * ncrows) / 2 + lp->ys;
    for (i = 0; i < 6; i++) {
      lp->shape.hexagon[i].x = (lp->xs - 1) * hexagonUnit[i].x;
      lp->shape.hexagon[i].y = ((lp->ys - 1) * hexagonUnit[i].y / 2) * 4 / 3;
    }
  } else {
		if (size < -MINSIZE)
			lp->ys = NRAND(MIN(-size, MAX(MINSIZE, MIN(lp->width, lp->height) /
					      MINGRIDSIZE)) - MINSIZE + 1) + MINSIZE;
		else if (size < MINSIZE) {
			if (!size)
				lp->ys = MAX(MINSIZE, MIN(lp->width, lp->height) / MINGRIDSIZE);
			else
				lp->ys = MINSIZE;
		} else
			lp->ys = MIN(size, MAX(MINSIZE, MIN(lp->width, lp->height) /
					       MINGRIDSIZE));
		lp->xs = lp->ys;
		lp->ncols = MAX(lp->width / lp->xs, ADAM_LOOPX + 1);
		lp->nrows = MAX(lp->height / lp->ys, ADAM_LOOPX + 1);
		lp->xb = (lp->width - lp->xs * lp->ncols) / 2;
		lp->yb = (lp->height - lp->ys * lp->nrows) / 2;
	}
	lp->bx = 1;
	lp->by = 1;
	lp->bncols = lp->ncols + 2 * lp->bx;
	lp->bnrows = lp->nrows + 2 * lp->by;

	MI_CLEARWINDOW(mi);

	if (lp->oldcells != NULL) {
		(void) free((void *) lp->oldcells);
		lp->oldcells = NULL;
	}
	if ((lp->oldcells = (unsigned char *) calloc(lp->bncols * lp->bnrows,
			sizeof (unsigned char))) == NULL) {
		free_loop(display, lp);
		return;
	}
	if (lp->newcells != NULL) {
		(void) free((void *) lp->newcells);
		lp->newcells = NULL;
	}
	if ((lp->newcells = (unsigned char *) calloc(lp->bncols * lp->bnrows,
			sizeof (unsigned char))) == NULL) {
		free_loop(display, lp);
		return;
	}
	if (!init_table()) {
		release_loop(mi);
		return;
	}
	lp->mincol = lp->bncols - 1;
	lp->minrow = lp->bnrows - 1;
	lp->maxcol = 0; 
	lp->maxrow = 0;
#ifndef DELAYDEBUGLOOP
	{
		int flaws = MI_COUNT(mi);

		if (flaws < 0)
			flaws = NRAND(-MI_COUNT(mi) + 1);
		for (i = 0; i < flaws; i++) {
			init_flaw(mi);
		}
		/* actual flaws might be less since the adam loop done next */
	}
#endif
	init_adam(mi);
}

void
draw_loop(ModeInfo * mi)
{
	int         offset, i, j;
	unsigned char *z, *znew;
	loopstruct *lp;

	if (loops == NULL)
		return;
	lp = &loops[MI_SCREEN(mi)];
	if (lp->newcells == NULL)
		return;

	MI_IS_DRAWN(mi) = True;
	lp->dead = True;
#ifdef DELAYDEBUGLOOP
	if (MI_COUNT(mi) && lp->generation > MI_COUNT(mi)) {
		(void) sleep(DELAYDEBUGLOOP);
	}
#endif

	for (j = lp->minrow; j <= lp->maxrow; j++) {
		for (i = lp->mincol; i <= lp->maxcol; i++) {
			offset = j * lp->bncols + i;
			z = lp->oldcells + offset;
			znew = lp->newcells + offset;
			if (*z != *znew) {
				lp->dead = False;
				*z = *znew;
				if (!addtolist(mi, i - lp->bx, j - lp->by, *znew))
					return;
				if (i == lp->mincol && i > lp->bx)
					lp->mincol--;
				if (j == lp->minrow && j > lp->by)
					lp->minrow--;
				if (i == lp->maxcol && i < lp->bncols - 2 * lp->bx)
					lp->maxcol++;
				if (j == lp->maxrow && j < lp->bnrows - 2 * lp->by)
					lp->maxrow++;
			}
		}
	}
	for (i = 0; i < COLORS; i++)
		if (!draw_state(mi, i)) {
			free_loop(MI_DISPLAY(mi), lp);
			return;
		}
	if (++lp->generation > MI_CYCLES(mi) || lp->dead) {
		init_loop(mi);
		return;
	} else
		do_gen(lp);

	if (lp->redrawing) {
		for (i = 0; i < REDRAWSTEP; i++) {
			if ((*(lp->oldcells + lp->redrawpos))) {
				drawcell(mi, lp->redrawpos % lp->bncols - lp->bx,
					 lp->redrawpos / lp->bncols - lp->by,
					 *(lp->oldcells + lp->redrawpos));
			}
			if (++(lp->redrawpos) >= lp->bncols * (lp->bnrows - lp->bx)) {
				lp->redrawing = 0;
				break;
			}
		}
	}
}

void
refresh_loop(ModeInfo * mi)
{
	loopstruct *lp;

	if (loops == NULL)
		return;
	lp = &loops[MI_SCREEN(mi)];

	MI_CLEARWINDOW(mi);
	lp->redrawing = 1;
	lp->redrawpos = lp->by * lp->ncols + lp->bx;
}

#endif /* MODE_loop */
