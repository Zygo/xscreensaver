/* -*- Mode: C; tab-width: 4 -*- */
/* penrose --- quasiperiodic tilings */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)penrose.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by Timo Korvola <tkorvola@dopey.hut.fi>
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
 * 10-May-97: Jamie Zawinski <jwz@netscape.com> compatible with xscreensaver
 * 09-Sep-96: Written.
 */

/*-
Be careful, this probably still has a few bugs (many of which may only
appear with a very low probability).  These are seen with -verbose .
If one of these are hit penrose will reinitialize.
*/

/*-
 * See Onoda, Steinhardt, DiVincenzo and Socolar in
 * Phys. Rev. Lett. 60, #25, 1988 or
 * Strandburg in Computers in Physics, Sep/Oct 1991.
 *
 * This implementation uses the simpler version of the growth
 * algorithm, i.e., if there are no forced vertices, a randomly chosen
 * tile is added to a randomly chosen vertex (no preference for those
 * 108 degree angles).
 *
 * There are two essential differences to the algorithm presented in
 * the literature: First, we do not allow the tiling to enclose an
 * untiled area.  Whenever this is in danger of happening, we just
 * do not add the tile, hoping for a better random choice the next
 * time.  Second, when choosing a vertex randomly, we will take
 * one that lies withing the viewport if available.  If this seems to
 * cause enclosures in the forced rule case, we will allow invisible
 * vertices to be chosen.
 *
 * Tiling is restarted whenever one of the following happens: there
 * are no incomplete vertices within the viewport or the tiling has
 * extended a window's length beyond the edge of the window
 * horizontally or vertically or forced rule choice has failed 100
 * times due to areas about to become enclosed.
 *
 */

#ifdef STANDALONE
#define PROGCLASS "Penrose"
#define HACK_INIT init_penrose
#define HACK_DRAW draw_penrose
#define penrose_opts xlockmore_opts
#define DEFAULTS "*delay: 10000 \n" \
 "*size: 40 \n" \
 "*ncolors: 64 \n" \
 "*fullrandom: True \n" \
 "*verbose: False \n"
#include "xlockmore.h"		/* from the xscreensaver distribution */
#else /* !STANDALONE */
#include "xlock.h"		/* from the xlockmore distribution */
#endif /* !STANDALONE */

#define DEF_AMMANN  "False"

static Bool ammann;

static XrmOptionDescRec opts[] =
{
	{"-ammann", ".penrose.ammann", XrmoptionNoArg, (caddr_t) "on"},
	{"+ammann", ".penrose.ammann", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
	{(caddr_t *) & ammann, "ammann", "Ammann", DEF_AMMANN, t_Bool}
};
static OptionStruct desc[] =
{
	{"-/+ammann", "turn on/off Ammann lines"}
};

ModeSpecOpt penrose_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   penrose_description =
{"penrose", "init_penrose", "draw_penrose", "release_penrose",
 "init_penrose", "init_penrose", NULL, &penrose_opts,
 10000, 1, 1, -40, 64, 1.0, "",
 "Shows Penrose's quasiperiodic tilings", 0, NULL};

#endif

/*-
 * Annoyingly the ANSI C library people have reserved all identifiers
 * ending with _t for future use.  Hence we use _c as a suffix for
 * typedefs (c for class, although this is not C++).
 */

#define MINSIZE 5

/*-
 * In theory one could fit 10 tiles to a single vertex.  However, the
 * vertex rules only allow at most seven tiles to meet at a vertex.
 */

#define CELEBRATE 31415		/* This causes a pause, an error occurred. */
#define COMPLETION 3141		/* This causes a pause, tiles filled up screen. */

#define MAX_TILES_PER_VERTEX 7
#define N_VERTEX_RULES 8
#define ALLOC_NODE( type) ((type *)malloc( sizeof( type)))

/*-
 * These are used to specify directions.  They can also be used in bit
 * masks to specify a combination of directions.
 */
#define S_LEFT 1
#define S_RIGHT 2


/*-
 * We do not actually maintain objects corresponding to the tiles since
 * we do not really need them and they would only consume memory and
 * cause additional bookkeeping.  Instead we only have vertices, and
 * each vertex lists the type of each adjacent tile as well as the
 * position of the vertex on the tile (hereafter refered to as
 * "corner").  These positions are numbered in counterclockwise order
 * so that 0 is where two double arrows meet (see one of the
 * articles).  The tile type and vertex number are stored in a single
 * integer (we use char, and even most of it remains unused).
 *
 * The primary use of tile objects would be draw traversal, but we do
 * not currently do redraws at all (we just start over).
 */
#define VT_CORNER_MASK 0x3
#define VT_TYPE_MASK 0x4
#define VT_THIN 0
#define VT_THICK 0x4
#define VT_BITS 3
#define VT_TOTAL_MASK 0x7

typedef unsigned char vertex_type_c;

/*-
 * These allow one to compute the types of the other corners of the tile.  If
 * you are standing at a vertex of type vt looking towards the middle of the
 * tile, VT_LEFT( vt) is the vertex on your left etc.
 */
#define VT_LEFT( vt) ((((vt) - 1) & VT_CORNER_MASK) | (((vt) & VT_TYPE_MASK)))
#define VT_RIGHT( vt) ((((vt) + 1) & VT_CORNER_MASK) | (((vt) & VT_TYPE_MASK)))
#define VT_FAR( vt) ((vt) ^ 2)


/*-
 * Since we do not do redraws, we only store the vertices we need.  These are
 * the ones with still some empty space around them for the growth algorithm
 * to fill.
 *
 * Here we use a doubly chained ring-like structure as vertices often need
 * to be removed or inserted (they are kept in geometrical order 
 * circling the tiled area counterclockwise).  The ring is refered to by
 * a pointer to one more or less random node.  When deleting nodes one
 * must make sure that this pointer continues to refer to a valid
 * node.  A vertex count is maintained to make it easier to pick
 * vertices randomly.
 */
typedef struct forced_node forced_node_c;

typedef struct fringe_node {
	struct fringe_node *prev;
	struct fringe_node *next;
	/* These are numbered counterclockwise.  The gap, if any, lies
	   between the last and first tiles.  */
	vertex_type_c tiles[MAX_TILES_PER_VERTEX];
	int         n_tiles;
	/* A bit mask used to indicate vertex rules that are still applicable for
	   completing this vertex.  Initialize this to (1 << N_VERTEX_RULES) - 1,
	   i.e., all ones, and the rule matching functions will automatically mask
	   out rules that no longer match. */
	unsigned char rule_mask;
	/* If the vertex is on the forced vertex list, this points to the
	   pointer to the appropriate node in the list.  To remove the
	   vertex from the list just set *list_ptr to the next node,
	   deallocate and decrement node count. */
	struct forced_node **list_ptr;
	/* Screen coordinates. */
	XPoint      loc;
	/* We also keep track of 5D coordinates to avoid rounding errors.
	   These are in units of edge length. */
	int         fived[5];
	/* This is used to quickly check if a vertex is visible. */
	unsigned char off_screen;
} fringe_node_c;

typedef struct {
	fringe_node_c *nodes;
	/* This does not count off-screen nodes. */
	int         n_nodes;
} fringe_c;


/*-
 * The forced vertex pool contains vertices where at least one
 * side of the tiled region can only be extended in one way.  Note
 * that this does not necessarily mean that there would only be one
 * applicable rule.  forced_sides are specified using S_LEFT and
 * S_RIGHT as if looking at the untiled region from the vertex.
 */
struct forced_node {
	fringe_node_c *vertex;
	unsigned    forced_sides;
	struct forced_node *next;
};

typedef struct {
	forced_node_c *first;
	int         n_nodes, n_visible;
} forced_pool_c;


/* This is the data related to the tiling of one screen. */
typedef struct {
	int         width, height;
	XPoint      origin;
	int         edge_length;
	fringe_c    fringe;
	forced_pool_c forced;
	int         done, failures;
	int         thick_color, thin_color;
	int         busyLoop;
	Bool        ammann;
} tiling_c;

static tiling_c *tilings;	/* = {0} */


/* The tiles are listed in counterclockwise order. */
typedef struct {
	vertex_type_c tiles[MAX_TILES_PER_VERTEX];
	int         n_tiles;
} vertex_rule_c;

static vertex_rule_c vertex_rules[N_VERTEX_RULES] =
{
	{
  {VT_THICK | 2, VT_THICK | 2, VT_THICK | 2, VT_THICK | 2, VT_THICK | 2}, 5},
	{
  {VT_THICK | 0, VT_THICK | 0, VT_THICK | 0, VT_THICK | 0, VT_THICK | 0}, 5},
	{
		{VT_THICK | 0, VT_THICK | 0, VT_THICK | 0, VT_THIN | 0}, 4},
	{
	 {VT_THICK | 2, VT_THICK | 2, VT_THIN | 1, VT_THIN | 3, VT_THICK | 2,
	  VT_THIN | 1, VT_THIN | 3}, 7},
	{
		{VT_THICK | 2, VT_THICK | 2, VT_THICK | 2, VT_THICK | 2,
		 VT_THIN | 1, VT_THIN | 3}, 6},
	{
		{VT_THICK | 1, VT_THICK | 3, VT_THIN | 2}, 3},
	{
		{VT_THICK | 0, VT_THIN | 0, VT_THIN | 0}, 3},
	{
     {VT_THICK | 2, VT_THIN | 1, VT_THICK | 3, VT_THICK | 1, VT_THIN | 3}, 5}
};


/* Match information returned by match_rules. */
typedef struct {
	int         rule;
	int         pos;
} rule_match_c;


/* Occasionally floating point coordinates are needed. */
typedef struct {
	float       x, y;
} fcoord_c;


/* All angles are measured in multiples of 36 degrees. */
typedef int angle_c;

static angle_c vtype_angles[] =
{4, 1, 4, 1, 2, 3, 2, 3};

#define vtype_angle( v) (vtype_angles[ v])


/* Direction angle of an edge. */
static      angle_c
vertex_dir(ModeInfo * mi, fringe_node_c * vertex, unsigned side)
{
	tiling_c   *tp = &tilings[MI_SCREEN(mi)];
	fringe_node_c *v2 =
	(side == S_LEFT ? vertex->next : vertex->prev);
	register int i;

	for (i = 0; i < 5; i++)
		switch (v2->fived[i] - vertex->fived[i]) {
			case 1:
				return 2 * i;
			case -1:
				return (2 * i + 5) % 10;
		}
	tp->done = True;
	if (MI_IS_VERBOSE(mi)) {
		(void) fprintf(stderr,
		       "Weirdness in vertex_dir (this has been reported)\n");
		for (i = 0; i < 5; i++)
			(void) fprintf(stderr, "v2->fived[%d]=%d, vertex->fived[%d]=%d\n",
				       i, v2->fived[i], i, vertex->fived[i]);
	}
	tp->busyLoop = CELEBRATE;
	return 0;
}


/* Move one step to a given direction. */
static void
add_unit_vec(angle_c dir, int *fived)
{
	static int  dir2i[] =
	{0, 3, 1, 4, 2};

	while (dir < 0)
		dir += 10;
	fived[dir2i[dir % 5]] += (dir % 2 ? -1 : 1);
}


/* For comparing coordinates. */
#define fived_equal( f1, f2) (!memcmp( (f1), (f2), 5 * sizeof( int)))


/*-
 * This computes screen coordinates from 5D representation.  Note that X
 * uses left-handed coordinates (y increases downwards).
 */
static      XPoint
fived_to_loc(int fived[], tiling_c * tp)
{
	static fcoord_c fived_table[5] =
	{
		{.0, .0}};
	float       fifth = 8 * atan(1.) / 5;
	register int i;
	register float r;
	register fcoord_c offset =
	{.0, .0};
	XPoint      pt = tp->origin;

	if (fived_table[0].x == .0)
		for (i = 0; i < 5; i++) {
			fived_table[i].x = cos(fifth * i);
			fived_table[i].y = sin(fifth * i);
		}
	for (i = 0; i < 5; i++) {
		r = fived[i] * tp->edge_length;
		offset.x += r * fived_table[i].x;
		offset.y -= r * fived_table[i].y;
	}
	pt.x += (int) (offset.x + .5);
	pt.y += (int) (offset.y + .5);
	return pt;
}


/* Mop up dynamic data for one screen. */
static void
release_screen(tiling_c * tp)
{
	register fringe_node_c *fp1, *fp2;
	register forced_node_c *lp1, *lp2;

	if (tp->fringe.nodes == 0)
		return;
	fp1 = tp->fringe.nodes;
	do {
		fp2 = fp1;
		fp1 = fp1->next;
		(void) free((void *) fp2);
	} while (fp1 != tp->fringe.nodes);
	tp->fringe.nodes = 0;
	for (lp1 = tp->forced.first; lp1 != 0;) {
		lp2 = lp1;
		lp1 = lp1->next;
		(void) free((void *) lp2);
	}
	tp->forced.first = 0;
}


/* Called to init the mode. */
void
init_penrose(ModeInfo * mi)
{
	tiling_c   *tp;
	fringe_node_c *fp;
	int         i, size;

	if (tilings == NULL) {
		if ((tilings = (tiling_c *) calloc(MI_NUM_SCREENS(mi),
						 sizeof (tiling_c))) == NULL)
			return;
	}
	tp = &tilings[MI_SCREEN(mi)];

	if (MI_IS_FULLRANDOM(mi))
		tp->ammann = (Bool) (LRAND() & 1);
	else
		tp->ammann = ammann;
	tp->done = False;
	tp->busyLoop = 0;
	tp->failures = 0;
	tp->width = MI_WIDTH(mi);
	tp->height = MI_HEIGHT(mi);
	if (MI_NPIXELS(mi) > 2) {
		tp->thick_color = NRAND(MI_NPIXELS(mi));
		/* Insure good contrast */
		tp->thin_color = (NRAND(2 * MI_NPIXELS(mi) / 3) + tp->thick_color +
				  MI_NPIXELS(mi) / 6) % MI_NPIXELS(mi);
	}
	size = MI_SIZE(mi);
	if (size < -MINSIZE)
		tp->edge_length = NRAND(MIN(-size, MAX(MINSIZE,
		   MIN(tp->width, tp->height) / 2)) - MINSIZE + 1) + MINSIZE;
	else if (size < MINSIZE) {
		if (!size)
			tp->edge_length = MAX(MINSIZE, MIN(tp->width, tp->height) / 2);
		else
			tp->edge_length = MINSIZE;
	} else
		tp->edge_length = MIN(size, MAX(MINSIZE,
					    MIN(tp->width, tp->height) / 2));
	tp->origin.x = (tp->width / 2 + NRAND(tp->width)) / 2;
	tp->origin.y = (tp->height / 2 + NRAND(tp->height)) / 2;
	tp->fringe.n_nodes = 2;
	if (tp->fringe.nodes != 0)
		release_screen(tp);
	if (tp->fringe.nodes != 0 || tp->forced.first != 0) {
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in init_penrose()\n");
			(void) fprintf(stderr, "tp->fringe.nodes = 0 && tp->forced.first = 0\n");
		}
		release_screen(tp);	/* Try again */
		tp->done = True;
	}
	tp->forced.n_nodes = tp->forced.n_visible = 0;
	fp = tp->fringe.nodes = ALLOC_NODE(fringe_node_c);
	if (fp == 0) {
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in init_penrose()\n");
			(void) fprintf(stderr, "fp = 0\n");
		}
		fp = tp->fringe.nodes = ALLOC_NODE(fringe_node_c);
		tp->done = True;
	}
	/* First vertex. */
	fp->rule_mask = (1 << N_VERTEX_RULES) - 1;
	fp->list_ptr = 0;
	fp->prev = fp->next = ALLOC_NODE(fringe_node_c);
	if (fp->next == 0) {
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in init_penrose()\n");
			(void) fprintf(stderr, "fp->next = 0\n");
		}
		fp->prev = fp->next = ALLOC_NODE(fringe_node_c);
		tp->done = True;
	}
	fp->n_tiles = 0;
	fp->loc = tp->origin;
	fp->off_screen = False;
	for (i = 0; i < 5; i++)
		fp->fived[i] = 0;

	/* Second vertex. */
	*(fp->next) = *fp;
	fp->next->prev = fp->next->next = fp;
	fp = fp->next;
	i = NRAND(5);
	fp->fived[i] = 2 * NRAND(2) - 1;
	fp->loc = fived_to_loc(fp->fived, tp);
	/* That's it!  We have created our first edge. */
}

/*-
 * This attempts to match the configuration of vertex with the vertex
 * rules.   The return value is a total match count.  If matches is
 * non-null, it will be used to store information about the matches
 * and must be large enough to contain it.  To play it absolutely
 * safe, allocate room for MAX_TILES_PER_VERTEX * N_VERTEX_RULES
 * entries when searching all matches.   The rule mask of vertex will
 * be applied and rules masked out will not be searched.  Only strict
 * subsequences match.  If first_only is true, the search stops when
 * the first match is found.  Otherwise all matches will be found and
 * the rule_mask of vertex will be updated, which also happens in
 * single-match mode if no match is found.
 */
static int
match_rules(fringe_node_c * vertex, rule_match_c * matches, int first_only)
{
	/* I will assume that I can fit all the relevant bits in vertex->tiles
	   into one unsigned long.  With 3 bits per element and at most 7
	   elements this means 21 bits, which should leave plenty of room.
	   After packing the bits the rest is just integer comparisons and
	   some bit shuffling.  This is essentially Rabin-Karp without
	   congruence arithmetic. */
	register int i, j;
	int         hits = 0, good_rules[N_VERTEX_RULES], n_good = 0;
	unsigned long
	            vertex_hash = 0, lower_bits_mask = ~(VT_TOTAL_MASK << VT_BITS * (vertex->n_tiles - 1));
	unsigned    new_rule_mask = 0;

	for (i = 0; i < N_VERTEX_RULES; i++)
		if (vertex->n_tiles >= vertex_rules[i].n_tiles)
			vertex->rule_mask &= ~(1 << i);
		else if (vertex->rule_mask & 1 << i)
			good_rules[n_good++] = i;
	for (i = 0; i < vertex->n_tiles; i++)
		vertex_hash |= (unsigned long) vertex->tiles[i] << (VT_BITS * i);

	for (j = 0; j < n_good; j++) {
		unsigned long rule_hash = 0;
		vertex_rule_c *vr = vertex_rules + good_rules[j];

		for (i = 0; i < vertex->n_tiles; i++)
			rule_hash |= (unsigned long) vr->tiles[i] << (VT_BITS * i);
		if (rule_hash == vertex_hash) {
			if (matches != 0) {
				matches[hits].rule = good_rules[j];
				matches[hits].pos = 0;
			}
			hits++;
			if (first_only)
				return hits;
			else
				new_rule_mask |= 1 << good_rules[j];
		}
		for (i = vr->n_tiles - 1; i > 0; i--) {
			rule_hash = vr->tiles[i] | (rule_hash & lower_bits_mask) << VT_BITS;
			if (vertex_hash == rule_hash) {
				if (matches != 0) {
					matches[hits].rule = good_rules[j];
					matches[hits].pos = i;
				}
				hits++;
				if (first_only)
					return hits;
				else
					new_rule_mask |= 1 << good_rules[j];
			}
		}
	}
	vertex->rule_mask = new_rule_mask;
	return hits;
}


/*-
 * find_completions finds the possible ways to add a tile to a vertex.
 * The return values is the number of such possibilities.  You must
 * first call match_rules to produce matches and n_matches.  sides
 * specifies which side of the vertex to extend and can be S_LEFT or
 * S_RIGHT.  If results is non-null, it should point to an array large
 * enough to contain the results, which will be stored there.
 * MAX_COMPL elements will always suffice.  If first_only is true we
 * stop as soon as we find one possibility (NOT USED).
 */
#define MAX_COMPL 2

static int
find_completions(fringe_node_c * vertex, rule_match_c * matches, int n_matches,
	       unsigned side, vertex_type_c * results /*, int first_only */ )
{
	int         n_res = 0, cont;
	register int i, j;
	vertex_type_c buf[MAX_COMPL];

	if (results == 0)
		results = buf;
	if (n_matches <= 0)
		return 0;
	for (i = 0; i < n_matches; i++) {
		vertex_rule_c *rule = vertex_rules + matches[i].rule;
		int         pos = (matches[i].pos
		   + (side == S_RIGHT ? vertex->n_tiles : rule->n_tiles - 1))
		% rule->n_tiles;
		vertex_type_c vtype = rule->tiles[pos];

		cont = 1;
		for (j = 0; j < n_res; j++)
			if (vtype == results[j]) {
				cont = 0;
				break;
			}
		if (cont)
			results[n_res++] = vtype;
	}
	return n_res;
}


/*-
 * Draw a tile on the display.  Vertices must be given in a
 * counterclockwise order.  vtype is the vertex type of v1 (and thus
 * also gives the tile type).
 */
static void
draw_tile(fringe_node_c * v1, fringe_node_c * v2,
	  fringe_node_c * v3, fringe_node_c * v4,
	  vertex_type_c vtype, ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	tiling_c   *tp = &tilings[MI_SCREEN(mi)];
	XPoint      pts[5];
	vertex_type_c corner = vtype & VT_CORNER_MASK;

	if (v1->off_screen && v2->off_screen && v3->off_screen && v4->off_screen)
		return;
	pts[corner] = v1->loc;
	pts[VT_RIGHT(corner)] = v2->loc;
	pts[VT_FAR(corner)] = v3->loc;
	pts[VT_LEFT(corner)] = v4->loc;
	pts[4] = pts[0];
	if (MI_NPIXELS(mi) > 2) {
		if ((vtype & VT_TYPE_MASK) == VT_THICK)
			XSetForeground(display, gc, MI_PIXEL(mi, tp->thick_color));
		else
			XSetForeground(display, gc, MI_PIXEL(mi, tp->thin_color));
	} else
		XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
	XFillPolygon(display, window, gc, pts, 4, Convex, CoordModeOrigin);
	XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	XDrawLines(display, window, gc, pts, 5, CoordModeOrigin);

	if (tp->ammann) {
		/* Draw some Ammann lines for debugging purposes.  This will probably
		   fail miserably on a b&w display. */

		if ((vtype & VT_TYPE_MASK) == VT_THICK) {
			static float r = .0;

			if (r == .0) {
				float       pi10 = 2 * atan(1.) / 5;

				r = 1 - sin(pi10) / (2 * sin(3 * pi10));
			}
			if (MI_NPIXELS(mi) > 2)
				XSetForeground(display, gc, MI_PIXEL(mi, tp->thin_color));
			else {
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				XSetLineAttributes(display, gc, 1, LineOnOffDash, CapNotLast, JoinMiter);
			}
			XDrawLine(display, window, gc,
			      (int) (r * pts[3].x + (1 - r) * pts[0].x + .5),
			      (int) (r * pts[3].y + (1 - r) * pts[0].y + .5),
			      (int) (r * pts[1].x + (1 - r) * pts[0].x + .5),
			     (int) (r * pts[1].y + (1 - r) * pts[0].y + .5));
			if (MI_NPIXELS(mi) <= 2)
				XSetLineAttributes(display, gc, 1, LineSolid, CapNotLast, JoinMiter);
		} else {
			if (MI_NPIXELS(mi) > 2)
				XSetForeground(display, gc, MI_PIXEL(mi, tp->thick_color));
			else {
				XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				XSetLineAttributes(display, gc, 1, LineOnOffDash, CapNotLast, JoinMiter);
			}
			XDrawLine(display, window, gc,
				  (int) ((pts[3].x + pts[2].x) / 2 + .5),
				  (int) ((pts[3].y + pts[2].y) / 2 + .5),
				  (int) ((pts[1].x + pts[2].x) / 2 + .5),
				  (int) ((pts[1].y + pts[2].y) / 2 + .5));
			if (MI_NPIXELS(mi) <= 2)
				XSetLineAttributes(display, gc, 1, LineSolid, CapNotLast, JoinMiter);
		}
	}
}

/*-
 * Update the status of this vertex on the forced vertex queue.  If
 * the vertex has become untileable set tp->done.  This is supposed
 * to detect dislocations -- never call this routine with a completely
 * tiled vertex.
 *
 * Check for untileable vertices in check_vertex and stop tiling as
 * soon as one finds one.  I don't know if it is possible to run out
 * of forced vertices while untileable vertices exist (or will
 * cavities inevitably appear).  If this can happen, add_random_tile
 * might get called with an untileable vertex, causing ( n <= 1).
 * (This is what the tp->done checks for).
 *
 * A delayLoop celebrates the dislocation.
 */
static void
check_vertex(ModeInfo * mi, fringe_node_c * vertex, tiling_c * tp)
{
	rule_match_c hits[MAX_TILES_PER_VERTEX * N_VERTEX_RULES];
	int         n_hits = match_rules(vertex, hits, False);
	unsigned    forced_sides = 0;

	if (vertex->rule_mask == 0) {
		tp->done = True;
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Dislocation occurred!\n");
		}
		tp->busyLoop = CELEBRATE;	/* Should be able to recover */
	}
	if (1 == find_completions(vertex, hits, n_hits, S_LEFT, 0 /*, False */ ))
		forced_sides |= S_LEFT;
	if (1 == find_completions(vertex, hits, n_hits, S_RIGHT, 0 /*, False */ ))
		forced_sides |= S_RIGHT;
	if (forced_sides == 0) {
		if (vertex->list_ptr != 0) {
			forced_node_c *node = *vertex->list_ptr;

			*vertex->list_ptr = node->next;
			if (node->next != 0)
				node->next->vertex->list_ptr = vertex->list_ptr;
			(void) free((void *) node);
			tp->forced.n_nodes--;
			if (!vertex->off_screen)
				tp->forced.n_visible--;
			vertex->list_ptr = 0;
		}
	} else {
		forced_node_c *node;

		if (vertex->list_ptr == 0) {
			node = ALLOC_NODE(forced_node_c);
			node->vertex = vertex;
			node->next = tp->forced.first;
			if (tp->forced.first != 0)
				tp->forced.first->vertex->list_ptr = &(node->next);
			tp->forced.first = node;
			vertex->list_ptr = &(tp->forced.first);
			tp->forced.n_nodes++;
			if (!vertex->off_screen)
				tp->forced.n_visible++;
		} else
			node = *vertex->list_ptr;
		node->forced_sides = forced_sides;
	}
}


/*-
 * Delete this vertex.  If the vertex is a member of the forced vertex queue,
 * also remove that entry.  We assume that the vertex is no longer
 * connected to the fringe.  Note that tp->fringe.nodes must not point to
 * the vertex being deleted.
 */
static void
delete_vertex(ModeInfo * mi, fringe_node_c * vertex, tiling_c * tp)
{
	if (tp->fringe.nodes == vertex) {
		tp->done = True;
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in delete_penrose()\n");
			(void) fprintf(stderr, "tp->fringe.nodes == vertex\n");
		}
		tp->busyLoop = CELEBRATE;
	}
	if (vertex->list_ptr != 0) {
		forced_node_c *node = *vertex->list_ptr;

		*vertex->list_ptr = node->next;
		if (node->next != 0)
			node->next->vertex->list_ptr = vertex->list_ptr;
		(void) free((void *) node);
		tp->forced.n_nodes--;
		if (!vertex->off_screen)
			tp->forced.n_visible--;
	}
	if (!vertex->off_screen)
		tp->fringe.n_nodes--;
	(void) free((void *) vertex);
}


/*-
 * Check whether the addition of a tile of type vtype would completely fill
 * the space available at vertex.
 */
static int
fills_vertex(ModeInfo * mi, vertex_type_c vtype, fringe_node_c * vertex)
{
	return
		(vertex_dir(mi, vertex, S_LEFT) - vertex_dir(mi, vertex, S_RIGHT)
		 - vtype_angle(vtype)) % 10 == 0;
}


/*-
 * If you were to add a tile of type vtype to a specified side of
 * vertex, fringe_changes tells you which other vertices it would
 * attach to.  The addresses of these vertices will be stored in the
 * last three arguments.  Null is stored if the corresponding vertex
 * would need to be allocated.
 *
 * The function also analyzes which vertices would be swallowed by the tiling
 * and thus cut off from the fringe.  The result is returned as a bit pattern.
 */
#define FC_BAG 1		/* Total enclosure.  Should never occur. */
#define FC_NEW_RIGHT 2
#define FC_NEW_FAR 4
#define FC_NEW_LEFT 8
#define FC_NEW_MASK 0xe
#define FC_CUT_THIS 0x10
#define FC_CUT_RIGHT 0x20
#define FC_CUT_FAR 0x40
#define FC_CUT_LEFT 0x80
#define FC_CUT_MASK 0xf0
#define FC_TOTAL_MASK 0xff

static unsigned
fringe_changes(ModeInfo * mi, fringe_node_c * vertex,
	       unsigned side, vertex_type_c vtype,
	       fringe_node_c ** right, fringe_node_c ** far,
	       fringe_node_c ** left)
{
	fringe_node_c *v, *f = NULL;
	unsigned    result = FC_NEW_FAR;	/* We clear this later if necessary. */

	if (far)
		*far = 0;
	if (fills_vertex(mi, vtype, vertex)) {
		result |= FC_CUT_THIS;
	} else if (side == S_LEFT) {
		result |= FC_NEW_RIGHT;
		if (right)
			*right = 0;
	} else {
		result |= FC_NEW_LEFT;
		if (left)
			*left = 0;
	}

	if (!(result & FC_NEW_LEFT)) {
		v = vertex->next;
		if (left)
			*left = v;
		if (fills_vertex(mi, VT_LEFT(vtype), v)) {
			result = (result & ~FC_NEW_FAR) | FC_CUT_LEFT;
			f = v->next;
			if (far)
				*far = f;
		}
	}
	if (!(result & FC_NEW_RIGHT)) {
		v = vertex->prev;
		if (right)
			*right = v;
		if (fills_vertex(mi, VT_RIGHT(vtype), v)) {
			result = (result & ~FC_NEW_FAR) | FC_CUT_RIGHT;
			f = v->prev;
			if (far)
				*far = f;
		}
	}
	if (!(result & FC_NEW_FAR)
	    && fills_vertex(mi, VT_FAR(vtype), f)) {
		result |= FC_CUT_FAR;
		result &= (~FC_NEW_LEFT & ~FC_NEW_RIGHT);
		if (right && (result & FC_CUT_LEFT))
			*right = f->next;
		if (left && (result & FC_CUT_RIGHT))
			*left = f->prev;
	}
	if (((result & FC_CUT_LEFT) && (result & FC_CUT_RIGHT))
	    || ((result & FC_CUT_THIS) && (result & FC_CUT_FAR)))
		result |= FC_BAG;
	return result;
}


/* A couple of lesser helper functions for add_tile. */
static void
add_vtype(fringe_node_c * vertex, unsigned side, vertex_type_c vtype)
{
	if (side == S_RIGHT)
		vertex->tiles[vertex->n_tiles++] = vtype;
	else {
		register int i;

		for (i = vertex->n_tiles; i > 0; i--)
			vertex->tiles[i] = vertex->tiles[i - 1];
		vertex->tiles[0] = vtype;
		vertex->n_tiles++;
	}
}

static fringe_node_c *
alloc_vertex(ModeInfo * mi, angle_c dir, fringe_node_c * from, tiling_c * tp)
{
	fringe_node_c *v = ALLOC_NODE(fringe_node_c);

	if (v == 0) {
		tp->done = True;
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in alloc_vertex()\n");
			(void) fprintf(stderr, "v = 0\n");
		}
		tp->busyLoop = CELEBRATE;
	}
	*v = *from;
	add_unit_vec(dir, v->fived);
	v->loc = fived_to_loc(v->fived, tp);
	if (v->loc.x < 0 || v->loc.y < 0
	    || v->loc.x >= tp->width || v->loc.y >= tp->height) {
		v->off_screen = True;
		if (v->loc.x < -tp->width || v->loc.y < -tp->height
		  || v->loc.x >= 2 * tp->width || v->loc.y >= 2 * tp->height)
			tp->done = True;
	} else {
		v->off_screen = False;
		tp->fringe.n_nodes++;
	}
	v->n_tiles = 0;
	v->rule_mask = (1 << N_VERTEX_RULES) - 1;
	v->list_ptr = 0;
	return v;
}

/*-
 * Add a tile described by vtype to the side of vertex.  This must be
 * allowed by the rules -- we do not check it here.  New vertices are
 * allocated as necessary.  The fringe and the forced vertex pool are updated.
 * The new tile is drawn on the display.
 *
 * One thing we do check here is whether the new tile causes an untiled
 * area to become enclosed by the tiling.  If this would happen, the tile
 * is not added.  The return value is true iff a tile was added.
 */
static int
add_tile(ModeInfo * mi,
	 fringe_node_c * vertex, unsigned side, vertex_type_c vtype)
{
	tiling_c   *tp = &tilings[MI_SCREEN(mi)];

	fringe_node_c
		* left = 0,
		*right = 0,
		*far = 0,
		*node;
	unsigned    fc = fringe_changes(mi, vertex, side, vtype, &right, &far, &left);

	vertex_type_c
		ltype = VT_LEFT(vtype),
		rtype = VT_RIGHT(vtype),
		ftype = VT_FAR(vtype);

	/* By our conventions vertex->next lies to the left of vertex and
	   vertex->prev to the right. */

	/* This should never occur. */
	if (fc & FC_BAG) {
		tp->done = True;
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in add_tile()\n");
			(void) fprintf(stderr, "fc = %d, FC_BAG = %d\n", fc, FC_BAG);
		}
	}
	if (side == S_LEFT) {
		if (right == 0)
			right = alloc_vertex(mi,
					     vertex_dir(mi, vertex, S_LEFT) - vtype_angle(vtype), vertex, tp);
		if (far == 0)
			far = alloc_vertex(mi,
					   vertex_dir(mi, left, S_RIGHT) + vtype_angle(ltype), left, tp);
	} else {
		if (left == 0)
			left = alloc_vertex(mi,
					    vertex_dir(mi, vertex, S_RIGHT) + vtype_angle(vtype), vertex, tp);
		if (far == 0)
			far = alloc_vertex(mi,
					   vertex_dir(mi, right, S_LEFT) - vtype_angle(rtype), right, tp);
	}

	/* Having allocated the new vertices, but before joining them with
	   the rest of the fringe, check if vertices with same coordinates
	   already exist.  If any such are found, give up. */
	node = tp->fringe.nodes;
	do {
		if (((fc & FC_NEW_LEFT) && fived_equal(node->fived, left->fived))
		    || ((fc & FC_NEW_RIGHT) && fived_equal(node->fived, right->fived))
		    || ((fc & FC_NEW_FAR) && fived_equal(node->fived, far->fived))) {
			/* Better luck next time. */
			if (fc & FC_NEW_LEFT)
				delete_vertex(mi, left, tp);
			if (fc & FC_NEW_RIGHT)
				delete_vertex(mi, right, tp);
			if (fc & FC_NEW_FAR)
				delete_vertex(mi, far, tp);
			return False;
		}
		node = node->next;
	} while (node != tp->fringe.nodes);

	/* Rechain. */
	if (!(fc & FC_CUT_THIS)) {
		if (side == S_LEFT) {
			vertex->next = right;
			right->prev = vertex;
		} else {
			vertex->prev = left;
			left->next = vertex;
		}
	}
	if (!(fc & FC_CUT_FAR)) {
		if (!(fc & FC_CUT_LEFT)) {
			far->next = left;
			left->prev = far;
		}
		if (!(fc & FC_CUT_RIGHT)) {
			far->prev = right;
			right->next = far;
		}
	}
	draw_tile(vertex, right, far, left, vtype, mi);

	/* Delete vertices that are no longer on the fringe.  Check the others. */
	if (fc & FC_CUT_THIS) {
		tp->fringe.nodes = far;
		delete_vertex(mi, vertex, tp);
	} else {
		add_vtype(vertex, side, vtype);
		check_vertex(mi, vertex, tp);
		tp->fringe.nodes = vertex;
	}
	if (fc & FC_CUT_FAR)
		delete_vertex(mi, far, tp);
	else {
		add_vtype(far, fc & FC_CUT_RIGHT ? S_LEFT : S_RIGHT, ftype);
		check_vertex(mi, far, tp);
	}
	if (fc & FC_CUT_LEFT)
		delete_vertex(mi, left, tp);
	else {
		add_vtype(left, fc & FC_CUT_FAR ? S_LEFT : S_RIGHT, ltype);
		check_vertex(mi, left, tp);
	}
	if (fc & FC_CUT_RIGHT)
		delete_vertex(mi, right, tp);
	else {
		add_vtype(right, fc & FC_CUT_FAR ? S_RIGHT : S_LEFT, rtype);
		check_vertex(mi, right, tp);
	}
	return True;
}


/*-
 * Add a forced tile to a given forced vertex.  Basically an easy job,
 * since we know what to add.  But it might fail if adding the tile
 * would cause some untiled area to become enclosed.  There is also another
 * more exotic culprit: we might have a dislocation.  Fortunately, they
 * are very rare (the PRL article reported that perfect tilings of over
 * 2^50 tiles had been generated).  There is a version of the algorithm
 * that doesn't produce dislocations, but it's a lot hairier than the
 * simpler version I used.
 */
static int
add_forced_tile(ModeInfo * mi, forced_node_c * node)
{
	tiling_c   *tp = &tilings[MI_SCREEN(mi)];
	unsigned    side;
	vertex_type_c vtype;
	rule_match_c hits[MAX_TILES_PER_VERTEX * N_VERTEX_RULES];
	int         n;

	if (node->forced_sides == (S_LEFT | S_RIGHT))
		side = NRAND(2) ? S_LEFT : S_RIGHT;
	else
		side = node->forced_sides;
	n = match_rules(node->vertex, hits, True);
	n = find_completions(node->vertex, hits, n, side, &vtype /*, True */ );
	if (n <= 0) {
		tp->done = True;
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in add_forced_tile()\n");
			(void) fprintf(stderr, "n = %d\n", n);
		}
	}
	return add_tile(mi, node->vertex, side, vtype);
}


/*-
 * Whether the addition of a tile of vtype on the given side of vertex
 * would conform to the rules.  The efficient way to do this would be
 * to add the new tile and then use the same type of search as in
 * match_rules.  However, this function is not a performance
 * bottleneck (only needed for random tile additions, which are
 * relatively infrequent), so I will settle for a simpler implementation.
 */
static int
legal_move(fringe_node_c * vertex, unsigned side, vertex_type_c vtype)
{
	rule_match_c hits[MAX_TILES_PER_VERTEX * N_VERTEX_RULES];
	vertex_type_c legal_vt[MAX_COMPL];
	int         n_hits, n_legal, i;

	n_hits = match_rules(vertex, hits, False);
	n_legal = find_completions(vertex, hits, n_hits, side, legal_vt /*, False */ );
	for (i = 0; i < n_legal; i++)
		if (legal_vt[i] == vtype)
			return True;
	return False;
}


/*-
 * Add a randomly chosen tile to a given vertex.  This requires more checking
 * as we must make sure the new tile conforms to the vertex rules at every
 * vertex it touches. */
static void
add_random_tile(fringe_node_c * vertex, ModeInfo * mi)
{
	fringe_node_c *right, *left, *far;
	int         i, j, n, n_hits, n_good;
	unsigned    side, fc, no_good, s;
	vertex_type_c vtypes[MAX_COMPL];
	rule_match_c hits[MAX_TILES_PER_VERTEX * N_VERTEX_RULES];
	tiling_c   *tp = &tilings[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) > 2) {
		tp->thick_color = NRAND(MI_NPIXELS(mi));
		/* Insure good contrast */
		tp->thin_color = (NRAND(2 * MI_NPIXELS(mi) / 3) + tp->thick_color +
				  MI_NPIXELS(mi) / 6) % MI_NPIXELS(mi);
	} else
		tp->thick_color = tp->thin_color = MI_WHITE_PIXEL(mi);
	n_hits = match_rules(vertex, hits, False);
	side = NRAND(2) ? S_LEFT : S_RIGHT;
	n = find_completions(vertex, hits, n_hits, side, vtypes /*, False */ );
	/* One answer would mean a forced tile. */
	if (n <= 0) {
		tp->done = True;
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in add_random_tile()\n");
			(void) fprintf(stderr, "n = %d\n", n);
		}
	}
	no_good = 0;
	n_good = n;
	for (i = 0; i < n; i++) {
		fc = fringe_changes(mi, vertex, side, vtypes[i], &right, &far, &left);
		if (fc & FC_BAG) {
			tp->done = True;
			if (MI_IS_VERBOSE(mi)) {
				(void) fprintf(stderr, "Weirdness in add_random_tile()\n");
				(void) fprintf(stderr, "fc = %d, FC_BAG = %d\n", fc, FC_BAG);
			}
		}
		if (right) {
			s = (((fc & FC_CUT_FAR) && (fc & FC_CUT_LEFT)) ? S_RIGHT : S_LEFT);
			if (!legal_move(right, s, VT_RIGHT(vtypes[i]))) {
				no_good |= (1 << i);
				n_good--;
				continue;
			}
		}
		if (left) {
			s = (((fc & FC_CUT_FAR) && (fc & FC_CUT_RIGHT)) ? S_LEFT : S_RIGHT);
			if (!legal_move(left, s, VT_LEFT(vtypes[i]))) {
				no_good |= (1 << i);
				n_good--;
				continue;
			}
		}
		if (far) {
			s = ((fc & FC_CUT_LEFT) ? S_RIGHT : S_LEFT);
			if (!legal_move(far, s, VT_FAR(vtypes[i]))) {
				no_good |= (1 << i);
				n_good--;
			}
		}
	}
	if (n_good <= 0) {
		tp->done = True;
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in add_random_tile()\n");
			(void) fprintf(stderr, "n_good = %d\n", n_good);
		}
	}
	n = NRAND(n_good);
	for (i = j = 0; i <= n; i++, j++)
		while (no_good & (1 << j))
			j++;

	i = add_tile(mi, vertex, side, vtypes[j - 1]);
	if (!i) {
		tp->done = True;
		if (MI_IS_VERBOSE(mi)) {
			(void) fprintf(stderr, "Weirdness in add_random_tile()\n");
			(void) fprintf(stderr, "i = %d\n", i);
		}
	}
}

/* One step of the growth algorithm. */
void
draw_penrose(ModeInfo * mi)
{
	tiling_c   *tp = &tilings[MI_SCREEN(mi)];
	int         i = 0, n;
	forced_node_c *p = tp->forced.first;

	if (tp->busyLoop > 0) {
		tp->busyLoop--;
		return;
	}
	if (tp->done || tp->failures >= 100) {
		init_penrose(mi);
		return;
	}
	/* Check for the initial "2-gon". */
	if (tp->fringe.nodes->prev == tp->fringe.nodes->next) {
		vertex_type_c vtype = VT_TOTAL_MASK & LRAND();

		MI_CLEARWINDOW(mi);

		(void) add_tile(mi, tp->fringe.nodes, S_LEFT, vtype);
		return;
	}
	/* No visible nodes left. */
	if (tp->fringe.n_nodes == 0) {
		tp->done = True;
		tp->busyLoop = COMPLETION;	/* Just finished drawing */
		return;
	}
	if (tp->forced.n_visible > 0 && tp->failures < 10) {
		n = NRAND(tp->forced.n_visible);
		for (;;) {
			while (p->vertex->off_screen)
				p = p->next;
			if (i++ < n)
				p = p->next;
			else
				break;
		}
	} else if (tp->forced.n_nodes > 0) {
		n = NRAND(tp->forced.n_nodes);
		while (i++ < n)
			p = p->next;
	} else {
		fringe_node_c *p = tp->fringe.nodes;

		n = NRAND(tp->fringe.n_nodes);
		i = 0;
		for (; i <= n; i++)
			do {
				p = p->next;
			} while (p->off_screen);
		add_random_tile(p, mi);
		tp->failures = 0;
		return;
	}
	if (add_forced_tile(mi, p))
		tp->failures = 0;
	else
		tp->failures++;
}


/* Total clean-up. */
void
release_penrose(ModeInfo * mi)
{
	if (tilings != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			tiling_c   *tp = &tilings[screen];

			release_screen(tp);
		}
		(void) free((void *) tilings);
		tilings = NULL;
	}
}
