/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef __PACMAN_H__
#define __PACMAN_H__

#include "config.h"
#include "xlockmoreI.h"

#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
#define USE_PIXMAP
#include "xpm-pixmap.h"
#else
#if defined(USE_PIXMAP)
#undef USE_PIXMAP
#endif
#endif

#define LEVHEIGHT 	32U
#define LEVWIDTH 	40U

#define GETNB(n) ((1 << (n)) - 1)
#define TESTNB(v, n) (((1 << (n)) & v) != 0x00)
#define SNB(v, n) ((v) |= (1 << (n)))
#define UNSNB(v, n) ((v) &= ~(1 << (n)))
#define GHOSTS 4U
#if defined(USE_PIXMAP)
#define MAXMOUTH 3
#else
#define MAXMOUTH 11
#endif
#define MAXGPOS 2
#define MAXGDIR 4
#define MAXGWAG 2
#define MAXGFLASH 2
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
#define PAC_DEATH_FRAMES 8

#define GHOST_TRACE ( LEVWIDTH * LEVHEIGHT )

typedef struct
{
    int vx, vy;
} tracevec_struct;

typedef enum
    { inbox = 0, goingout, randdir, chasing, hiding, goingin } GhostState;
typedef enum
    { ps_eating = 0, ps_chasing, ps_hiding, ps_random, ps_dieing } PacmanState;
typedef enum
{ GHOST_DANGER, GHOST_EATEN } GameState;

typedef struct
{
    volatile unsigned int col, row;
    unsigned int lastbox, nextcol, nextrow;
    int dead;
    int cfactor, rfactor;
    int cf, rf;
    int oldcf, oldrf;
    volatile int timeleft;
    GhostState aistate;
    int speed;
    XPoint delta;
    XPoint err;
    int flash_scared;
    int trace_idx;
    tracevec_struct trace[GHOST_TRACE];
    int home_idx;
    volatile int home_count;
    tracevec_struct way_home[GHOST_TRACE];
    volatile int wait_pos; /* a cycle before calculating the position */
#if 0  /* Used for debugging */
    int ndirs;
    int oldndirs;
#endif

#if 0 /* Used for debugging */
    char last_stat[1024];
#endif

} ghoststruct;

typedef struct
{
    unsigned int col, row;
    unsigned int lastbox, nextcol, nextrow;
    int mouthstage, mouthdirection;
    int cfactor, rfactor;
    int cf, rf;
    int oldcf, oldrf;
    int oldlx, oldly;
    int justate;
    PacmanState aistate;
    tracevec_struct trace[TRACEVECS];
    int cur_trace;
    int state_change;
    int roundscore;
    int speed;
    int lastturn;
    XPoint delta;
    XPoint err;
    int deaths;
    int init_row;
} pacmanstruct;

typedef struct
{
    unsigned short width, height;
    unsigned short nrows, ncols;
    short xs, ys, xb, yb;
    short incx, incy;
    GC stippledGC;
    int graphics_format;
    pacmanstruct pacman;
    ghoststruct *ghosts;
    unsigned int nghosts;
    Pixmap pacmanPixmap[4][MAXMOUTH];
    Pixmap pacmanMask[4][MAXMOUTH];
    Pixmap pacman_ds[PAC_DEATH_FRAMES]; /* pacman death sequence */
    Pixmap pacman_ds_mask[PAC_DEATH_FRAMES];
    Pixmap ghostPixmap[4][MAXGDIR][MAXGWAG];
    Pixmap ghostMask;
    Pixmap s_ghostPixmap[MAXGFLASH][MAXGWAG];   /* Scared ghost Pixmaps */
    Pixmap ghostEyes[MAXGDIR];
    char level[LEVHEIGHT * LEVWIDTH];
    unsigned int wallwidth;
    unsigned int dotsleft;
    int spritexs, spriteys, spritedx, spritedy;

    GameState gamestate;
    unsigned int timeleft;
} pacmangamestruct;

#define DIRVECS 4

extern pacmangamestruct *pacmangames;
extern Bool trackmouse;

typedef char lev_t[LEVHEIGHT][LEVWIDTH + 1];

#define NUM_BONUS_DOTS 4

#endif /* __PACMAN_H__ */
