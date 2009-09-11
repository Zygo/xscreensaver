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
 */

#include <assert.h>
#include "pacman.h"
#include "pacman_level.h"


#define NONE 0x0000
#define LT   0x1000
#define RT   0x0001
#define RB   0x0010
#define LB   0x0100
#define ALL  0x1111

#define BLOCK_EMPTY	' '
#define BLOCK_DOT_1	'`'
#define BLOCK_DOT_2	'.'
#define BLOCK_WALL	'#'
#define BLOCK_GHOST_ONLY	'='
#define BLOCK_WALL_TL	'\''
#define BLOCK_WALL_TR	'`'
#define BLOCK_WALL_BR	','
#define BLOCK_WALL_BL	'_'
#define BLOCK_WALL_HO	'-'
#define BLOCK_WALL_VE	'|'
#define BLOCK_DOT_BONUS 'o'

/* This is more or less the standard pacman level (without the left-right
   tunnel. */
static const lev_t stdlevel = {
    "########################################",
    "########################################",
    "#######````````````##````````````#######",
    "#######`####`#####`##`#####`####`#######",
    "#######`####`#####`##`#####`####`#######",
    "#######`####`#####`##`#####`####`#######",
    "#######``````````````````````````#######",
    "#######`####`##`########`##`####`#######",
    "#######`####`##`########`##`####`#######",
    "#######``````##````##````##``````#######",
    "############`#####`##`#####`############",
    "############`#####`##`#####`############",
    "############`##``````````##`############",
    "############`##`###==###`##`############",
    "############`##`########`##`############",
    "############````########````############",
    "############`##`########`##`############",
    "############`##`########`##`############",
    "############`##``````````##`############",
    "############`##`########`##`############",
    "############`##`########`##`############",
    "#######````````````##````````````#######",
    "#######`####`#####`##`#####`####`#######",
    "#######`####`#####`##`#####`####`#######",
    "#######```##````````````````##```#######",
    "#########`##`##`########`##`##`#########",
    "#########`##`##`########`##`##`#########",
    "#######``````##````##````##``````#######",
    "#######`##########`##`##########`#######",
    "#######`##########`##`##########`#######",
    "#######``````````````````````````#######",
    "########################################"
};

#define TILES_COUNT 11U

#define GO_UP 0x0001U
#define GO_LEFT 0x0002U
#define GO_RIGHT 0x0004U
#define GO_DOWN 0x0008U

static const struct tiles def_tiles[TILES_COUNT] = {
/*   
 *   ' ' == dont care == BLOCK_EMPTY
 *   '#' == set wall, and not clear == BLOCK_WALL
 *   '`' == clear == BLOCK_DOT_1
 *   middle position is always set as cleardef
 */
    {
        "  #  " "  #  " " ``` " "  #  " "  #  ", {
    GO_LEFT, GO_RIGHT, 0, 0}, 2,
            (unsigned) (1 << 0 | 1 << 6 | 1 << 8 | 1 << 10)}, {
        "     " "  `  " "##`##" "  `  " "     ", {
    GO_UP, GO_DOWN, 0, 0}, 2,
            (unsigned) (1 << 1 | 1 << 7 | 1 << 9 | 1 << 10)}, {
        "   ##" "##`##" "##`` " "#### " "#### ", {
    GO_UP, GO_RIGHT, 0, 0}, 2,
            (unsigned) (1 << 2 | 1 << 6 | 1 << 7 | 1 << 10)}, {
        "#### " "#### " "##`` " "##`##" "   ##", {
    GO_RIGHT, GO_DOWN, 0, 0}, 2,
            (unsigned) (1 << 3 | 1 << 7 | 1 << 8 | 1 << 10)}, {
        "  ###" "  ###" " ``##" "##`  " "##   ", {
    GO_LEFT, GO_DOWN, 0, 0}, 2,
            (unsigned) (1 << 4 | 1 << 8 | 1 << 9 | 1 << 10)}, {
        "##   " "##`##" " ``##" " ####" " ####", {
    GO_LEFT, GO_UP, 0, 0}, 2,
            (unsigned) (1 << 5 | 1 << 6 | 1 << 9 | 1 << 10)}, {
        "##`##" "##`##" "`````" " ### " " ### ", {
    GO_LEFT, GO_UP, GO_RIGHT, 0}, 3, (unsigned) 1 << 6}, {
        "  `##" "##`##" "##```" "##`##" "  `##", {
    GO_UP, GO_RIGHT, GO_DOWN, 0}, 3, (unsigned) (1 << 7)}, {
        " ### " " ### " "`````" "##`##" "##`##", {
    GO_LEFT, GO_RIGHT, GO_DOWN, 0}, 3, (unsigned) (1 << 8)}, {
        "##`  " "##`##" "```##" "##`##" "##`  ", {
    GO_UP, GO_DOWN, GO_LEFT, 0}, 3, (unsigned) (1 << 9)}, {
        "##`##" "##`##" "`````" "##`##" "##`##", {
    GO_UP, GO_DOWN, GO_LEFT, GO_RIGHT}, 4, (unsigned) (1 << 10)}
};

/* probability array for each of the tiles */
#define MAXTILEPROB 22
static const unsigned tileprob[MAXTILEPROB] =
    { 0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 6, 6, 7, 7, 8, 8, 8, 9, 9, 10, 10, 10 };


static int creatlevelblock (pacmangamestruct *pp, 
                            lev_t * level, const unsigned x,
                            const unsigned y);


enum
{ TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

/* Sets a block in the level to a certain state. */
static void
setblockto (lev_t * level, const unsigned x, const unsigned y, const char c)
{
    if (!(x < LEVWIDTH && y < LEVHEIGHT))
        return;
    (*level)[y][x] = c;
}

/* check if a block is set */
static int
checkset (lev_t * level, const unsigned x, const unsigned y)
{
    if (!(x < LEVWIDTH && y < LEVHEIGHT) ||
        (*level)[y][x] == BLOCK_WALL || (*level)[y][x] == BLOCK_GHOST_ONLY)
        return True;
    return False;
}

/* Check if a block is not set */
static int
checksetout (lev_t * level, const unsigned x, const unsigned y)
{
    if (!(x < LEVWIDTH && y < LEVHEIGHT) || checkset (level, x, y) != 0)
        return True;

    return False;
}

/* Check if a block cannot be set */
static int
checkunsetdef (lev_t * level, const unsigned x, const unsigned y)
{
    if (!(x < LEVWIDTH && y < LEVHEIGHT))
        return False;
    if ((*level)[y][x] == BLOCK_DOT_1)
        return True;
    return False;
}

/* Initializes a level to empty state. */
static void
clearlevel (lev_t * level)
{
    unsigned x, y;

    for (y = 0; y < LEVHEIGHT; y++)
        for (x = 0; x < LEVWIDTH; x++)
            (*level)[y][x] = BLOCK_EMPTY;
}

/* Changes a level from the level creation structure ((array to array) to
   array. */
static void
copylevel (char *dest, lev_t * level)
{
    unsigned x, y;

    for (y = 0; y < LEVHEIGHT; y++)
        for (x = 0; x < LEVWIDTH; x++)
            dest[y * LEVWIDTH + x] = (*level)[y][x];
}

/* Creates a jail to work around, so we can finish it later. */
static void
createjail (lev_t * level, const unsigned width, const unsigned height)
{
    unsigned x, y, xstart, xend, ystart, yend;

    if (LEVWIDTH < width || LEVHEIGHT < height)
        return;

    xstart = LEVWIDTH / 2 - width / 2;
    xend = LEVWIDTH / 2 + width / 2;
    ystart = LEVHEIGHT / 2 - height / 2;
    yend = LEVHEIGHT / 2 + height / 2;

    for (y = ystart - 1; y < yend + 1; y++)
        for (x = xstart - 1; x < xend + 1; x++)
            setblockto (level, x, y, BLOCK_DOT_1);

    for (y = ystart; y < yend; y++)
        for (x = xstart; x < xend; x++)
            setblockto (level, x, y, BLOCK_WALL);
}

void
get_jail_opening ( int *x, int *y)
{
    int xstart = LEVWIDTH / 2  - JAILWIDTH / 2;
    int ystart = LEVHEIGHT / 2 - JAILHEIGHT / 2;
    *x = xstart + JAILWIDTH / 2;
    *y = ystart;
}

/* Finishes a jail so it is empty and the ghostpass is on top. */
static void
finishjail (lev_t * level, const unsigned width, const unsigned height)
{
    unsigned x, y, xstart, xend, ystart, yend;

    xstart = LEVWIDTH / 2 - width / 2;
    xend = LEVWIDTH / 2 + width / 2;
    ystart = LEVHEIGHT / 2 - height / 2;
    yend = LEVHEIGHT / 2 + height / 2;

    for (y = ystart + 1; y < yend - 1; y++)
        for (x = xstart + 1; x < xend - 1; x++)
            setblockto (level, x, y, BLOCK_EMPTY);

    for (x = xstart - 1; x < xend + 1; x++) {
        setblockto (level, x, ystart - 1, BLOCK_EMPTY);
        setblockto (level, x, yend, BLOCK_EMPTY);
    }

    for (y = ystart - 1; y < yend + 1; y++) {
        setblockto (level, xstart - 1, y, BLOCK_EMPTY);
        setblockto (level, xend, y, BLOCK_EMPTY);
    }

    setblockto (level, xstart + width / 2 - 1, ystart, BLOCK_GHOST_ONLY);
    setblockto (level, xstart + width / 2, ystart, BLOCK_GHOST_ONLY);
}

/* Tries to set a block at a certain position. Returns true if possible,
    and leaves level in new state (plus block), or False if not possible,
    and leaves level in unpredictable state. */
static int
tryset (lev_t * level, const unsigned xpos, const unsigned ypos,
        const char *block)
{
    register unsigned x, y;
    register char locchar;
    int xstart, ystart;
    unsigned xend, yend;

    if ((*level)[ypos][xpos] == BLOCK_DOT_1)
        return False;

    xstart = xpos - 2;
    ystart = ypos - 2;

    for (y = 0; y < TILEHEIGHT; y++)
        for (x = 0; x < TILEWIDTH; x++) {
            locchar = block[y * TILEWIDTH + x];
            if (locchar == BLOCK_EMPTY)
                continue;
            if (locchar == BLOCK_DOT_1 &&
                (xstart + x < 1 ||
                 xstart + x >= LEVWIDTH - 1 ||
                 ystart + y < 1 ||
                 ystart + y >= LEVHEIGHT - 1 ||
                 checkset (level, xstart + x, ystart + y) != 0))
                return False;
            else if (locchar == BLOCK_WALL &&
                     (xstart + x > 1 &&
                      xstart + x < LEVWIDTH &&
                      ystart + y > 1 &&
                      ystart + y < LEVHEIGHT - 1) &&
                     checkunsetdef (level,
                                    (unsigned) (xstart + x),
                                    (unsigned) (ystart + y)) != 0)
                return False;
        }

    /* and set the block in place */

    xend = (xstart + TILEWIDTH < LEVWIDTH - 1) ?
        TILEWIDTH : LEVWIDTH - xstart - 2;
    yend = (ystart + TILEHEIGHT < LEVHEIGHT - 1) ?
        TILEHEIGHT : LEVHEIGHT - ystart - 2;

    for (y = (ystart < 1) ? (unsigned) (1 - ystart) : 0U; y < yend; y++)
        for (x = (xstart < 1) ? (unsigned) (1 - xstart) : 0U; x < xend; x++) {
            locchar = block[y * TILEWIDTH + x];
            if ((locchar == BLOCK_WALL) &&
                ((*level)[ystart + y][xstart + x] == BLOCK_EMPTY)) {
                (*level)[ystart + y][xstart + x] = BLOCK_WALL;
                (*level)[ystart + y]
                    [LEVWIDTH - (xstart + x + 1)] = BLOCK_WALL;
            }
        }

    (*level)[ypos][xpos] = BLOCK_DOT_1;
    (*level)[ypos][LEVWIDTH - xpos - 1] = BLOCK_DOT_1;

    return True;
}

/* Tries certain combinations of blocks in the level recursively. */
static unsigned
nextstep (pacmangamestruct *pp, 
          lev_t * level, const unsigned x, const unsigned y,
          unsigned dirvec[], unsigned ndirs)
{
    unsigned dirpos, curdir, inc = 0;
    int ret = 0;

    while (ndirs > 0) {
        ndirs--;
        if (ndirs == 0) {
            curdir = dirvec[0];
        }
        else {
            dirpos = NRAND (ndirs);
            curdir = dirvec[dirpos];
            /* nope, no bufoverflow, but ndirs - 1 + 1 */
            dirvec[dirpos] = dirvec[ndirs];
            dirvec[ndirs] = curdir;
        }

        switch (curdir) {
        case GO_UP:
            if (y < 1 || (ret = creatlevelblock (pp, level, x, y - 1))
                == 0)
                return 0;
            break;
        case GO_RIGHT:
            if (x > LEVWIDTH - 2 || (ret = creatlevelblock (pp, level, x + 1, y))
                == 0)
                return 0;
            break;
        case GO_DOWN:
            if (y > LEVHEIGHT - 2 || (ret = creatlevelblock (pp, level, x, y + 1))
                == 0)
                return 0;
            break;
        case GO_LEFT:
            if (x < 1 || (ret = creatlevelblock (pp, level, x - 1, y))
                == 0)
                return 0;
        }
        if (ret != -1)
            inc += (unsigned) ret;
    }
    if (inc == 0)
        inc = 1;
    return inc;
}

static int
creatlevelblock (pacmangamestruct *pp, 
                 lev_t * level, const unsigned x, const unsigned y)
{
    unsigned tried = GETNB (TILES_COUNT);
    unsigned tilenr;
    unsigned ret;
    lev_t savedlev;

    if (!pp->tiles) {
        pp->tiles = (struct tiles *) malloc (sizeof (def_tiles));
        memcpy (pp->tiles, def_tiles, sizeof (def_tiles));
    }

    if (!((x < LEVWIDTH) && (y < LEVHEIGHT)))
        return 0;

    if (checkunsetdef (level, x, y) != 0)
        return -1;

    if (x == 0)
        tried &= ~(1 << 0);
    else if (x == 1)
        tried &= ~(1 << 4 | 1 << 5 | 1 << 6 | 1 << 8 | 1 << 9 | 1 << 10);
    else if (x == LEVWIDTH - 1)
        tried &= ~(1 << 0);
    else if (x == LEVWIDTH - 2)
        tried &= ~(1 << 2 | 1 << 3 | 1 << 6 | 1 << 7 | 1 << 8 | 1 << 10);

    if (y == 1)
        tried &= ~(1 << 2 | 1 << 5 | 1 << 6 | 1 << 7 | 1 << 9 | 1 << 10);
    else if (y == 0)
        tried &= ~(1 << 1);
    else if (y == LEVHEIGHT - 1)
        tried &= ~(1 << 1);
    else if (y == LEVHEIGHT - 2)
        tried &= ~(1 << 3 | 1 << 4 | 1 << 7 | 1 << 8 | 1 << 9 | 1 << 10);

    /* make a copy of the current level, so we can go back on the stack */
    (void) memcpy (&savedlev, level, sizeof (lev_t));

    /* while there are still some blocks left to try */
    while (tried != 0x00) {
        tilenr = tileprob[NRAND (MAXTILEPROB)];

        if (!TESTNB (tried, tilenr))
            continue;

        if (tryset (level, x, y, pp->tiles[tilenr].block) != 0) {
            if ((ret = nextstep (pp, level, x, y, pp->tiles[tilenr].dirvec,
                                 pp->tiles[tilenr].ndirs)) != 0) {
                return ret + 1;
            }
            (void) memcpy (level, &savedlev, sizeof (lev_t));
        }
        tried &= ~(pp->tiles[tilenr].simular_to);
    }
    return 0;
}

/* Fills up all empty space so there is wall everywhere. */
static void
filllevel (lev_t * level)
{
    unsigned x, y;

    for (y = 0; y < LEVHEIGHT; y++)
        for (x = 0; x < LEVWIDTH; x++)
            if ((*level)[y][x] == BLOCK_EMPTY)
                (*level)[y][x] = BLOCK_WALL;
}


/* Check to see if the x and y value are in the corners.
 * we could probable compute these values once and avoid
 * go through the loops every time.
 */
static void
top_left (lev_t * level, unsigned int *passed_x, unsigned int *passed_y)
{
    int x, y;
    for (y = 0; y < LEVHEIGHT; y++)
        for (x = 0; x < LEVWIDTH; x++) {
            if (checkset (level, x, y) == 0) {
                *passed_x = x;
                *passed_y = y;
		return;
            }
        }
}


static void
bottom_left (lev_t * level, unsigned int *passed_x, unsigned int *passed_y)
{
    int x, y;
    for (y = LEVHEIGHT; y > -1; y--)
        for (x = 0; x < LEVWIDTH; x++) {
            if (checkset (level, x, y) == 0) {
                *passed_x = x;
                *passed_y = y;
                return;
            }
        }
}

static void
top_right (lev_t * level, unsigned int *passed_x, unsigned int *passed_y)
{
    int x, y;

    for (y = 0; y < LEVHEIGHT; y++)
        for (x = LEVWIDTH; x >= 0; x--) {
            if (checkset (level, x, y) == 0) {
                *passed_x = x;
                *passed_y = y;
		return;
            }
        }
}

static void
bottom_right (lev_t * level, unsigned int *passed_x, unsigned int *passed_y)
{
    int x, y;

    for (y = LEVHEIGHT; y >= 0; y--)
        for (x = LEVWIDTH; x >= 0; x--) {
            if (checkset (level, x, y) == 0) {
                *passed_x = x;
                *passed_y = y;
		return;
            }
        }
}

static void
init_bonus_dots (pacmangamestruct *pp, lev_t * level)
{
    unsigned int x = 0, y = 0;
    top_left (level, &x, &y);
    pp->bonus_dots[TOP_LEFT].x = x;
    pp->bonus_dots[TOP_LEFT].y = y;
    pp->bonus_dots[TOP_LEFT].eaten = False;
    top_right (level, &x, &y);
    pp->bonus_dots[TOP_RIGHT].x = x;
    pp->bonus_dots[TOP_RIGHT].y = y;
    pp->bonus_dots[TOP_RIGHT].eaten = False;
    bottom_left (level, &x, &y);
    pp->bonus_dots[BOTTOM_LEFT].x = x;
    pp->bonus_dots[BOTTOM_LEFT].y = y;
    pp->bonus_dots[BOTTOM_LEFT].eaten = False;
    bottom_right (level, &x, &y);
    pp->bonus_dots[BOTTOM_RIGHT].x = x;
    pp->bonus_dots[BOTTOM_RIGHT].y = y;
    pp->bonus_dots[BOTTOM_RIGHT].eaten = False;
}

int
is_bonus_dot (pacmangamestruct *pp, int x, int y, int *idx)
{
    int ret = False;
    int i;
    for (i = 0; i <= NUM_BONUS_DOTS; i++) {
/*     fprintf(stderr,"is bonus: passed x (%d, %d) bonus (%d, %d)\n",x,y,bonus_dots[i].x, bonus_dots[i].y); */
        if (x == pp->bonus_dots[i].x && y == pp->bonus_dots[i].y) {
            ret = True;
            *idx = i;
            break;
        }
    }
    return ret;
}

static void
check_bonus_idx (int idx)
{
    assert (0 <= idx && idx <= NUM_BONUS_DOTS);
}

int
bonus_dot_eaten (pacmangamestruct *pp, int idx)
{
    check_bonus_idx (idx);
    return pp->bonus_dots[idx].eaten;
}

void
eat_bonus_dot (pacmangamestruct *pp, int idx)
{
    check_bonus_idx (idx);
    pp->bonus_dots[idx].eaten = True;
}

void
bonus_dot_pos (pacmangamestruct *pp, int idx, int *x, int *y)
{
    check_bonus_idx (idx);
    *x = pp->bonus_dots[idx].x;
    *y = pp->bonus_dots[idx].y;
}

/* Changes a level from a simple wall/nowall to a wall with rounded corners
   and such.  Stupid algorithm, could be done better! */
static void
frmtlevel (pacmangamestruct *pp, lev_t * level)
{
    lev_t frmtlev;
    int x, y;
    int idx;
    register unsigned poscond;
    register unsigned poscond2;

    clearlevel (&frmtlev);
    init_bonus_dots (pp, level);
    for (y = 0; y < LEVHEIGHT; y++)
        for (x = 0; x < LEVWIDTH; x++) {

            if (checkset (level, x, y) == 0) {
                if (is_bonus_dot (pp, x, y, &idx)) {
                    frmtlev[y][x] = BLOCK_DOT_BONUS;
                }
                else {
                    frmtlev[y][x] = BLOCK_DOT_2;
                }
                continue;
            }

            if ((*level)[y][x] == BLOCK_GHOST_ONLY) {
                frmtlev[y][x] = BLOCK_GHOST_ONLY;
                continue;
            }

            poscond =
                (checksetout (level, x - 1, y - 1) != 0 ?
                 0x01U : 0U) |
                (checksetout (level, x + 1, y - 1) != 0 ?
                 0x02U : 0U) |
                (checksetout (level, x + 1, y + 1) != 0 ?
                 0x04U : 0U) |
                (checksetout (level, x - 1, y + 1) != 0 ? 0x08U : 0U);

            poscond2 =
                (checksetout (level, x - 1, y) != 0 ?
                 0x01U : 0) |
                (checksetout (level, x, y - 1) != 0 ?
                 0x02U : 0) |
                (checksetout (level, x + 1, y) != 0 ?
                 0x04U : 0) |
                (checksetout (level, x, y + 1) != 0 ? 0x08U : 0);

            switch (poscond) {
                /* completely filled */
            case 0x01U | 0x02U | 0x04U | 0x08U:
                frmtlev[y][x] = BLOCK_EMPTY;
                continue;

                /* left to top corner */
            case 0x01U:
                frmtlev[y][x] = BLOCK_WALL_TL;
                continue;
                /* top to right corner */
            case 0x02U:
                frmtlev[y][x] = BLOCK_WALL_TR;
                continue;
                /* right to bottom corner */
            case 0x04U:
                frmtlev[y][x] = BLOCK_WALL_BR;
                continue;
                /* bottom to left corner */
            case 0x08U:
                frmtlev[y][x] = BLOCK_WALL_BL;
                continue;
            }

            switch (poscond2) {
            case 0x01U | 0x04U:
            case 0x01U | 0x04U | 0x08U:
            case 0x01U | 0x04U | 0x02U:
                frmtlev[y][x] = BLOCK_WALL_HO;
                continue;
            case 0x02U | 0x08U:
            case 0x02U | 0x08U | 0x01U:
            case 0x02U | 0x08U | 0x04U:
                frmtlev[y][x] = BLOCK_WALL_VE;
                continue;
            case 0x01U | 0x02U:
                frmtlev[y][x] = BLOCK_WALL_TL;
                continue;
            case 0x02U | 0x04U:
                frmtlev[y][x] = BLOCK_WALL_TR;
                continue;
            case 0x04U | 0x08U:
                frmtlev[y][x] = BLOCK_WALL_BR;
                continue;
            case 0x08U | 0x01U:
                frmtlev[y][x] = BLOCK_WALL_BL;
                continue;
            }
            switch (poscond) {
            case 0x02U | 0x04U | 0x08U:
                frmtlev[y][x] = BLOCK_WALL_TL;
                continue;
            case 0x01U | 0x04U | 0x08U:
                frmtlev[y][x] = BLOCK_WALL_TR;
                continue;
            case 0x01U | 0x02U | 0x08U:
                frmtlev[y][x] = BLOCK_WALL_BR;
                continue;
            case 0x01U | 0x02U | 0x04U:
                frmtlev[y][x] = BLOCK_WALL_BL;
                continue;
            }
            frmtlev[y][x] = BLOCK_EMPTY;
        }
    (void) memcpy ((lev_t *) level, (lev_t *) & frmtlev, sizeof (lev_t));
}

/* Counts the number of dots in the level, and returns that number. */
static unsigned
countdots (pacmangamestruct *pp)
{
    unsigned i, count = 0;

    for (i = 0; i < LEVWIDTH * LEVHEIGHT; i++)
      if (pp->level[i] == BLOCK_DOT_2 || pp->level[i] == BLOCK_DOT_BONUS)
            count++;

    return count;
}

/* Creates a new level, and places that in the pacmangamestruct. */
int
createnewlevel (pacmangamestruct *pp)
{
    lev_t *level;
    unsigned dirvec[1] = { GO_UP };
    unsigned ret = 0, i = 0;

    if ((level = (lev_t *) calloc (1, sizeof (lev_t))) == NULL)
        return i;

    if (NRAND (2) == 0) {

        do {
            clearlevel (level);
            createjail (level, JAILWIDTH, JAILHEIGHT);
            if ((ret = nextstep (pp, level, LEVWIDTH / 2 - 1,
                                 LEVHEIGHT / 2 - JAILHEIGHT / 2 - 3,
                                 dirvec, 1)) == 0) {
                (void) free ((void *) level);
                return i;
            }
        } while (ret * 100 < (LEVWIDTH * LEVHEIGHT * MINDOTPERC));

        filllevel (level);
        frmtlevel (pp, level);
        finishjail (level, JAILWIDTH, JAILHEIGHT);
    }
    else {
        (void) memcpy (level, stdlevel, sizeof (lev_t));
        frmtlevel (pp, level);
        i = 1;
    }
    copylevel (pp->level, level);
    pp->dotsleft = countdots (pp);

    (void) free ((void *) level);
    return i;
}

/* Checks if a position is allowable for ghosts/pacs to enter. */
int
check_pos (pacmangamestruct * pp, int y, int x, int ghostpass)
{
    if ((pp->level[y * LEVWIDTH + x] == BLOCK_DOT_2) ||
        (pp->level[y * LEVWIDTH + x] == BLOCK_EMPTY) ||
        (pp->level[y * LEVWIDTH + x] == BLOCK_DOT_BONUS) ||
        ((pp->level[y * LEVWIDTH + x] == BLOCK_GHOST_ONLY) && ghostpass)) {
        return 1;
    }
    return 0;
}

/* Checks if there is a dot on the specified position in the level. */
int
check_dot (pacmangamestruct * pp, unsigned int x, unsigned int y)
{
    if (x >= LEVWIDTH || y >= LEVHEIGHT)
        return 0;
    if (pp->level[y * LEVWIDTH + x] == BLOCK_DOT_2)
        return 1;
    return 0;
}
