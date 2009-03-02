/*-
 * Copyright (c) 2002 by Edwin de Jong <mauddib@gmx.net>.
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
 *  3-May-2002: Added AI to pacman and ghosts, slowed down ghosts.
 * 26-Nov-2001: Random level generator added
 * 01-Nov-2000: Allocation checks
 * 04-Jun-1997: Compatible with xscreensaver
 *
 */

#define LEVHEIGHT 	32U
#define LEVWIDTH 	40U

#define GETNB(n) ((1 << (n)) - 1)
#define TESTNB(v, n) (((1 << (n)) & v) != 0x00)
#define SNB(v, n) ((v) |= (1 << (n)))
#define UNSNB(v, n) ((v) &= ~(1 << (n)))
#define GHOSTS 4U
#define MAXMOUTH 11
#define MAXGPOS 2
#define MINGRIDSIZE 4
#define MINSIZE 3
#define NOWHERE 16383
#define START ((LRAND() & 1) ? 1 : 3)
#define MINDOTPERC 10

#define YELLOW (MI_NPIXELS(mi) / 6)
#define GREEN (23 * MI_NPIXELS(mi) / 64)
#define BLUE (45 * MI_NPIXELS(mi) / 64)
#define WHITE (MI_NPIXELS(mi))

#define LINEWIDTH	4
#define HLINEWIDTH	1
#define JAILHEIGHT      7
#define JAILWIDTH       8

#define GETFACTOR(x, y) ((x) > (y) ? 1 : ((x) < (y) ? -1 : 0))
#define SIGN(x) GETFACTOR((x), 0)
#define TRACEVECS 40

#define DEF_TRACKMOUSE "False"
static Bool trackmouse;

typedef struct { int vx, vy; } tracevec_struct;

typedef enum { inbox = 0, goingout, randdir, chasing, hiding } GhostState;
typedef enum { ps_eating = 0, ps_chasing, ps_hiding, ps_random } PacmanState;
typedef enum { GHOST_DANGER, GHOST_EATEN } GameState;

typedef struct {
	unsigned int 	col, row;
	unsigned int 	lastbox, nextcol, nextrow;
	int		dead;
	int		cfactor, rfactor;
	int		cf, rf;
	int		oldcf, oldrf;
	int		timeleft;
	GhostState	aistate;
	/*int         color; */
	int		speed;
	XPoint	    	delta;
	XPoint		err;
} ghoststruct;

typedef struct {
	unsigned int	col, row;
	unsigned int	lastbox, nextcol, nextrow;
	int		mouthstage, mouthdirection;
	int		cfactor, rfactor;
	int		cf, rf;
	int		oldcf, oldrf;
	int		oldlx, oldly;
	int		justate;
	PacmanState	aistate;
	tracevec_struct	trace[TRACEVECS];
	int		cur_trace;
	int		state_change;
	int		roundscore;
	int		speed;
	int		lastturn;
	XPoint	    	delta;
	XPoint		err;
} pacmanstruct;

typedef struct {
	unsigned short 	width, height;
	unsigned short 	nrows, ncols;
	short 		xs, ys, xb, yb;
	short 		incx, incy;
	GC          	stippledGC;
	int         	graphics_format;
	pacmanstruct	pacman;
	ghoststruct	*ghosts;
	unsigned int	nghosts;
	Pixmap      	pacmanPixmap[4][MAXMOUTH];
/*	Pixmap	    	ghostPixmap[4][MAXGPOS];*/
	Pixmap	    	ghostPixmap;
	char        	level[LEVHEIGHT * LEVWIDTH];
	unsigned int	wallwidth;
	unsigned int	dotsleft;
	int		spritexs, spriteys, spritedx, spritedy;

	GameState	gamestate;
	unsigned int	timeleft;
} pacmangamestruct;

#define DIRVECS 4
static const struct { int dx, dy; } dirvecs[DIRVECS] =
	{ {-1, 0}, {0, 1}, {1, 0}, {0, -1}};

static pacmangamestruct *pacmangames = (pacmangamestruct *) NULL;

typedef char lev_t[LEVHEIGHT][LEVWIDTH + 1];

/* function type definitions */

	/* level generation */
static int 	creatlevelblock(lev_t *level, const unsigned x, 
				const unsigned y);
static void 	setblockto(lev_t *level, const unsigned x, const unsigned y, 
				const char c);
static int 	checkset(lev_t *level, const unsigned x, const unsigned y);
static int 	checksetout(lev_t *level, const unsigned x, const unsigned y);
static int 	checkunsetdef(lev_t *level, const unsigned x, const unsigned y);
static void 	clearlevel(lev_t *level);
static void 	copylevel(char *dest, lev_t *level);
static void 	createjail(lev_t *level, const unsigned width, 
				const unsigned height);
static void 	finishjail(lev_t *level, const unsigned width, 
				const unsigned height);
static int 	tryset(lev_t *level, const unsigned xpos, const unsigned ypos, 
				const char *block);
static int 	creatlevelblock(lev_t *level, const unsigned x, 
				const unsigned y);
static void 	filllevel(lev_t *level);
static void 	frmtlevel(lev_t *level);
static unsigned countdots(ModeInfo * mi);
static int 	createnewlevel(ModeInfo * mi);
static int	check_pos(pacmangamestruct *pp, int y, int x, int ghostpass);
static int 	check_dot(pacmangamestruct *pp, unsigned int x, unsigned int y);

	/* graphics rendering */
static void 	drawlevel(ModeInfo * mi);

	/* AI */
static int 	ghost_get_posdirs(pacmangamestruct *pp, int *posdirs, 
				ghoststruct *g);
static void 	ghost_random(pacmangamestruct *pp, ghoststruct *g);
static void 	ghost_chasing(pacmangamestruct *pp, ghoststruct *g);
static void 	ghost_hiding(pacmangamestruct *pp, ghoststruct *g);
static void 	ghost_update(pacmangamestruct *pp, ghoststruct *g);

static void 	pac_dot_vec(pacmangamestruct *pp, pacmanstruct *p, 
				long *vx, long *vy);
static int 	pac_ghost_prox_and_vector(pacmangamestruct *pp, 
				pacmanstruct *p, int *vx, int *vy);
static int 	pac_get_posdirs(pacmangamestruct *pp, pacmanstruct *p, 
				int *posdirs);
static void 	pac_clear_trace(pacmanstruct *p);
static void 	pac_save_trace(pacmanstruct *p, const int vx, const int vy);
static int 	pac_check_trace(const pacmanstruct *p, const int vx, 
				const int vy);
static void 	pac_eating(pacmangamestruct *pp, pacmanstruct *p);
#if 0
static void 	pac_chasing(pacmangamestruct *pp, pacmanstruct *p);
#endif
static void 	pac_random(pacmangamestruct *pp, pacmanstruct *p);
static int	pac_get_vector_screen(pacmangamestruct *pp, pacmanstruct *p, 
			const int x, const int y, int *vx, int *vy);
static int 	pac_trackmouse(ModeInfo * mi, pacmangamestruct *pp, 
			pacmanstruct *p);
static void 	pac_update(ModeInfo * mi, pacmangamestruct *pp, 
			pacmanstruct *p);

	/* generic functions */
static void 	repopulate(ModeInfo * mi);
