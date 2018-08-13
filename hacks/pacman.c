/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* pacman --- Mr. Pacman and his ghost friends */

#if 0
static const char sccsid[] = "@(#)pacman.c	5.00 2000/11/01 xlockmore";
#endif

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
 * 25-Feb-2005: Added bonus dots. I am using a recursive back track algorithm
 *              to help the ghost find there way home. This also means that
 *              they do not know the shorts path.
 *              Jeremy English jhe@jeremyenglish.org
 * 15-Aug-2004: Added support for pixmap pacman. 
 *              Jeremy English jhe@jeremyenglish.org
 * 11-Aug-2004: Added support for pixmap ghost.
 *              Jeremy English jhe@jeremyenglish.org
 * 13-May-2002: Added -trackmouse feature thanks to code from 'maze.c'.  
 *		splitted up code into several files.  Retouched AI code, cleaned
 *		up code.
 *  3-May-2002: Added AI to pacman and ghosts, slowed down ghosts.
 * 26-Nov-2001: Random level generator added
 * 01-Nov-2000: Allocation checks
 * 04-Jun-1997: Compatible with xscreensaver
 *
 */

/* TODO:
   1. think of a better level generation algorithm
*/

#define DEF_TRACKMOUSE "False"

#ifdef STANDALONE
#	define MODE_pacman
#	define DEFAULTS "*delay:   10000 \n" \
		 			"*size:    0     \n" \
 					"*ncolors: 6     \n" \
 					"*fpsTop: true   \n" \
 					"*fpsSolid: true \n" \

#	define UNIFORM_COLORS
#	define BRIGHT_COLORS
#	define release_pacman 0
#   define pacman_handle_event 0
#	include "xlockmore.h"   /* in xscreensaver distribution */
#   include <assert.h>
#else /* STANDALONE */
#	include "xlock.h"       /* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_pacman

#include "pacman.h"
#include "pacman_ai.h"
#include "pacman_level.h"
#include "images/gen/pacman_png.h"

#ifdef DISABLE_INTERACTIVE
ENTRYPOINT ModeSpecOpt pacman_opts = {
    0,
    (XrmOptionDescRec *) NULL,
    0,
    (argtype *) NULL,
    (OptionStruct *) NULL
};
#else
static XrmOptionDescRec opts[] = {
    {"-trackmouse", ".pacman.trackmouse", XrmoptionNoArg, "on"},
    {"+trackmouse", ".pacman.trackmouse", XrmoptionNoArg, "off"}
};

static argtype vars[] = {
    {&pacman_trackmouse, "trackmouse", "TrackMouse", DEF_TRACKMOUSE, t_Bool}
};

static OptionStruct desc[] = {
    {"-/+trackmouse", "turn on/off the tracking of the mouse"}
};

ENTRYPOINT ModeSpecOpt pacman_opts =
    { sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars,
desc };
#endif

#ifdef USE_MODULES
ModStruct pacman_description = {
    "pacman",                   /* *cmdline_arg; */
    "init_pacman",              /* *init_name; */
    "draw_pacman",              /* *callback_name; */
    (char *) NULL,              /* *release_name; */
    "refresh_pacman",           /* *refresh_name; */
    "change_pacman",            /* *change_name; */
    "free_pacman",              /* *free_name; */
    &pacman_opts,               /* *msopts */
    10000, 4, 1, 0, 64, 1.0, "", "Shows Pacman(tm)", 0, NULL
};

#endif

Bool pacman_trackmouse;
pacmangamestruct *pacman_games = (pacmangamestruct *) NULL;

static void repopulate (ModeInfo * mi);
static void drawlevel (ModeInfo * mi);


ENTRYPOINT void
free_pacman (ModeInfo * mi)
{
    Display * display = MI_DISPLAY (mi);
    pacmangamestruct * pp = &pacman_games[MI_SCREEN (mi)];
    int dir, mouth, i, j, k;

    if (pp->ghosts != NULL) {
        free (pp->ghosts);
        pp->ghosts = (ghoststruct *) NULL;
    }
    if (pp->stippledGC != None) {
        XFreeGC (display, pp->stippledGC);
        pp->stippledGC = None;
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < MAXGDIR; j++) {
            for (k = 0; k < MAXGWAG; k++) {
                if (pp->ghostPixmap[i][j][k] != None) {
                    XFreePixmap (display, pp->ghostPixmap[i][j][k]);
                    pp->ghostPixmap[i][j][k] = None;
                }
            }
        }
    }
    for (dir = 0; dir < 4; dir++)
        for (mouth = 0; mouth < MAXMOUTH; mouth++)
            if (pp->pacmanPixmap[dir][mouth] != None) {
                XFreePixmap (display, pp->pacmanPixmap[dir][mouth]);
                pp->pacmanPixmap[dir][mouth] = None;
            }
}

/* set pacman and the ghost in there starting positions, but don't draw a new
 level */
static void
reset_level (ModeInfo * mi, int n, int pac_init)
{
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    unsigned int ghost;

    XClearWindow (MI_DISPLAY(mi), MI_WINDOW(mi));
    drawlevel (mi);

    pp->gamestate = GHOST_DANGER;

    if ( pac_init ){
        pp->pacman.row = (LEVHEIGHT + JAILHEIGHT) / 2 - n;
        pp->pacman.init_row = pp->pacman.row;
    }
    else{
        pp->pacman.row = pp->pacman.init_row;
    }
    pp->pacman.col = (LEVWIDTH / 2);
    pp->pacman.nextrow = NOWHERE;
    pp->pacman.nextcol = NOWHERE;
    pp->pacman.cf = NOWHERE;
    pp->pacman.rf = NOWHERE;
    pp->pacman.oldcf = NOWHERE;
    pp->pacman.oldrf = NOWHERE;
    pp->pacman.oldlx = NOWHERE;
    pp->pacman.oldly = NOWHERE;
    pp->pacman.aistate = ps_eating;
    pp->pacman.cur_trace = 0;
    pp->pacman.roundscore = 0;
    pp->pacman.speed = 4;
    pp->pacman.lastturn = 0;
    pp->pacman.delta.x = 0;
    pp->pacman.delta.y = 0;

    for (ghost = 0; ghost < pp->nghosts; ghost++) {
        pp->ghosts[ghost].col = (LEVWIDTH / 2);
        pp->ghosts[ghost].row = (LEVHEIGHT / 2);
        pp->ghosts[ghost].nextcol = NOWHERE;
        pp->ghosts[ghost].nextrow = NOWHERE;
        pp->ghosts[ghost].dead = 0;
        pp->ghosts[ghost].lastbox = START;
        pp->ghosts[ghost].cf = NOWHERE;
        pp->ghosts[ghost].rf = NOWHERE;
        pp->ghosts[ghost].oldcf = NOWHERE;
        pp->ghosts[ghost].oldrf = NOWHERE;
        pp->ghosts[ghost].aistate = inbox;
        pp->ghosts[ghost].timeleft = ghost * 20;
        pp->ghosts[ghost].speed = 3;
        pp->ghosts[ghost].delta.x = 0;
        pp->ghosts[ghost].delta.y = 0;
        pp->ghosts[ghost].flash_scared = False;
        pp->ghosts[ghost].wait_pos = False;
        pacman_ghost_update (pp, &(pp->ghosts[ghost]));
    }
    pacman_update (mi, pp, &(pp->pacman));
}

static int
pacman_ghost_collision(unsigned int ghost, pacmangamestruct * pp)
{
    return (((pp->ghosts[ghost].nextrow == pp->pacman.nextrow) &&
             (pp->ghosts[ghost].nextcol == pp->pacman.nextcol)) ||
            ((pp->ghosts[ghost].nextrow == pp->pacman.row) &&
             (pp->ghosts[ghost].nextcol == pp->pacman.col) &&
             (pp->ghosts[ghost].row == pp->pacman.nextrow) &&
             (pp->ghosts[ghost].col == pp->pacman.nextcol)));
}


/* Checks for death of any ghosts/pacman and updates.  It also makes a new
   level if all ghosts are dead or all dots are eaten. */
static void
check_death (ModeInfo * mi, pacmangamestruct * pp)
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    unsigned int ghost;

    if (pp->pacman.aistate == ps_dieing) return;

    for (ghost = 0; ghost < pp->nghosts; ghost++) {

        /* The ghost have to be scared before you can kill them */
        if ( pacman_ghost_collision ( ghost, pp ) ) {
            if (pp->ghosts[ghost].aistate == goingin) continue;

            if (pp->ghosts[ghost].aistate == hiding) {
                pp->ghosts[ghost].dead = 1;
                pp->ghosts[ghost].aistate = goingin;
                pp->ghosts[ghost].wait_pos = True;
                XSetForeground (display, pp->stippledGC, MI_BLACK_PIXEL (mi));
                XFillRectangle (display, window,
                                pp->stippledGC,
                                pp->ghosts[ghost].cf,
                                pp->ghosts[ghost].rf,
                                pp->spritexs, pp->spriteys);
            }
            /* DIE PACMAN... */
            else {
                pp->pacman.deaths++;
                pp->pacman.aistate = ps_dieing;

            }
            continue;
        }
    }
}

/* Resets state of ghosts + pacman.  Creates a new level, draws that level. */
static void
repopulate (ModeInfo * mi)
{
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    pp->pacman.deaths = 0;
    reset_level (mi, pacman_createnewlevel (pp), True);
    check_death (mi, pp);
}

/* Sets the color to the color of a wall. */
static void
setwallcolor (ModeInfo * mi)
{
    Display *display = MI_DISPLAY (mi);
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];

    if (MI_NPIXELS (mi) > 2)
        XSetForeground (display, pp->stippledGC, MI_PIXEL (mi, BLUE));
    else
        XSetForeground (display, pp->stippledGC, MI_WHITE_PIXEL (mi));
}

/* Sets the color to the color of a dot. */
static void
setdotcolor (ModeInfo * mi)
{
    Display *display = MI_DISPLAY (mi);
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];

    XSetForeground (display, pp->stippledGC, MI_WHITE_PIXEL (mi));
}

static void
cleardotcolor (ModeInfo * mi)
{
    Display *display = MI_DISPLAY (mi);
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];

    XSetForeground (display, pp->stippledGC, MI_BLACK_PIXEL (mi));
}

#if 0
static void
draw_position (ModeInfo * mi, int x, int y, int color)
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    XFontStruct *font = NULL;
    char *f_name = "-*-utopia-*-r-*-*-*-600-*-*-p-*-*-*";
    char *s = NULL;

    font = load_font_retry (display, f_name);
    assert (font != NULL);

    s = (char *) malloc (256);
    assert (s != NULL);
    sprintf (s, "(%d,%d)", x, y);
    XSetForeground (display, pp->stippledGC, color);
    XDrawString (display, window, pp->stippledGC, x, y, s, strlen (s));
    free (s);
    free (font);
}
#endif
#if 0
static void
draw_number (ModeInfo * mi, int x, int y, int num, int color)
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    XFontStruct *font = NULL;
    char *f_name = "-*-utopia-*-r-*-*-*-600-*-*-p-*-*-*";
    char *s = NULL;

    font = load_font_retry (display, f_name);
    assert (font != NULL);

    s = (char *) malloc (256);
    assert (s != NULL);
    sprintf (s, "%d", num);
    XSetForeground (display, pp->stippledGC, color);
    XDrawString (display, window, pp->stippledGC, x, y, s, strlen (s));
    free (s);
    free (font);
}
#endif

#if 0
/* draw_grid - draws a grid on top of the playing field.
 * Used so that I can determine if I'm converting from rows and columns to x and y 
 * coordinates correctly.
 */
static void
draw_grid (ModeInfo * mi)
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    int h = MI_HEIGHT (mi);
    int w = MI_WIDTH (mi);
    int y = 0;
    int x = 0;
    XSetForeground (display, pp->stippledGC, 0xff0000);
    while (y < h) {
        while (x < w) {
            XDrawLine (display, window, pp->stippledGC, x, 0, x, h);
            x += 10;
        }
        x = 0;
        XDrawLine (display, window, pp->stippledGC, 0, y, w, y);
        y += 10;
    }
}
#endif

#if 0
static void
draw_string (ModeInfo * mi, int x, int y, char *s, int color)
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    XFontStruct *font = NULL;
    char *f_name = "-*-utopia-*-r-*-*-*-600-*-*-p-*-*-*";

    font = load_font_retry (display, f_name);
    assert (font != NULL);

    assert (s != NULL);
    XSetForeground (display, pp->stippledGC, color);
    XDrawString (display, window, pp->stippledGC, x, y, s, strlen (s));
    free (font);
}

/* I think this function has a memory leak. Be careful if you enable it. */
/* I only used it briefly to help me debug the ghost's aistate. It prints */
/* the state of each ghost on the left hand side of the screen */
static void
print_ghost_stats (ModeInfo *mi, ghoststruct *g , int ghost_num)
{
    char s[1024];

    sprintf (s, "GHOST: %d", ghost_num ); 
    switch (g->aistate){
    case inbox: 
        sprintf (s, "%s inbox", s);
        break;
    case goingout:
        sprintf (s, "%s goingout", s);
        break;
    case randdir:
        sprintf (s, "%s randdir", s);
        break;
    case chasing:
        sprintf (s, "%s chasing", s);
        break;
    case hiding:
        sprintf (s, "%s hiding", s);
        break;
    case goingin:
        sprintf (s, "%s goingin",s);
        break;
    }
    draw_string (mi, 0, (ghost_num *3) *10+50, g->last_stat, 0x000000);
    draw_string (mi, 0, (ghost_num *3) *10+50, s, 0xff0000);
    strcpy(g->last_stat,s);
}

/* prints the number of times pacman has died and his aistate on the left hand */
/* side of the screen */
static void
print_pac_stats ( ModeInfo *mi, pacmanstruct *pac )
{
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    char s[1024];
    sprintf (s, "Pacman, Deaths: %d", pac->deaths );
    switch ( pac->aistate ){
    case ps_eating:
        sprintf(s, "%s ps_eating",s );
        break;
    case ps_chasing:
        sprintf(s, "%s ps_chasing",s );
        break;
    case ps_hiding:
        sprintf(s, "%s ps_hiding",s );
        break;
    case ps_random:
        sprintf(s, "%s ps_random",s );
        break;
    case ps_dieing:
        sprintf(s, "%s ps_dieing",s );
        break;
    }
    draw_string ( mi, 0, 200, pp->last_pac_stat, 0x000000);
    draw_string ( mi, 0, 200, s, 0xff0000);
    strcpy(pp->last_pac_stat, s );
}

#endif

/*Ok, yeah whatever?*/
/*dot_rc_to_pixel - magic that converts row and columns into
 *the x and y coordinates of the screen.
 */
static void
dot_rc_to_pixel (ModeInfo * mi, int *x, int *y)
{
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    *x = (pp->xs * *x) +
        (pp->xs / 2) - (pp->xs > 32 ? (pp->xs / 16) : 1) + pp->xb;
    *y = (pp->ys * *y) +
        (pp->ys / 2) - (pp->ys > 32 ? (pp->ys / 16) : 1) + pp->yb;
}

/* dot_width_height - magic used to get the width and height of
 * a dot. This dot can also be scaled by a value.
 */
static void
dot_width_height (ModeInfo *mi, int *w, int *h)
{
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    if (pp->xs > 32){
        *w = *h = (pp->xs / 16 );
    }else {
        *w = *h = 1;
    }
}

static void
bonus_dot_width_height (ModeInfo *mi, int *w, int *h )
{
    *w = *h = MI_HEIGHT (mi) / 65;
}



static void
draw_dot (ModeInfo * mi, pacmangamestruct * pp, int x, int y,
          void (*width_height)(ModeInfo * mi, int *w, int *h), 
          int (*arc_func) (Display * display, Drawable d, GC gc,
                                      int x, int y, unsigned int width,
                                      unsigned int height, int angle1,
                                      int angle2))
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    int w, h;
    dot_rc_to_pixel (mi, &x, &y);
    width_height(mi, &w, &h);
    (void) arc_func (display, window, pp->stippledGC,
                     x, y, w, h, 0, 23040);
}

static void
draw_bonus_dot (ModeInfo * mi, pacmangamestruct * pp, int x, int y)
{
    int x2 = x;
    int y2 = y;
    setdotcolor (mi);
    draw_dot (mi, pp, x, y, bonus_dot_width_height,  XFillArc);
    dot_rc_to_pixel (mi, &x2, &y2);
#if 0
    draw_position (mi, x2, y2, 0xff0000);
#endif
}

static void
clear_bonus_dot (ModeInfo * mi, pacmangamestruct * pp, int x, int y)
{
    cleardotcolor (mi);
    draw_dot (mi, pp, x, y, bonus_dot_width_height, XFillArc);
}

static void
draw_regular_dot (ModeInfo * mi, pacmangamestruct * pp, int x, int y)
{
    setdotcolor (mi);
    draw_dot (mi, pp, x, y, dot_width_height, XDrawArc);
}

/* Draws a block in the level at the specified x and y locations. */
static void
drawlevelblock (ModeInfo * mi, pacmangamestruct * pp,
                const unsigned x, const unsigned y)
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    int dx = 0, dy = 0;

    if (pp->xs % 2 == 1)
        dx = -1;
    if (pp->ys % 2 == 1)
        dy = -1;

#ifndef HAVE_JWXYZ
    XSetFillStyle (display, pp->stippledGC, FillSolid);
#endif /* !HAVE_JWXYZ */
    XSetLineAttributes (display, pp->stippledGC, pp->wallwidth,
                        LineSolid, CapRound, JoinMiter);

    if (pp->xs < 2 || pp->ys < 2) {
        switch (pp->level[y * LEVWIDTH + x]) {
        case ' ':
        case '=':
            break;
        case '.':
            setdotcolor (mi);
            (void) XDrawPoint (display, window,
                               pp->stippledGC,
                               x * pp->xs + pp->xb, y * pp->ys + pp->yb);
            break;
        default:
            setwallcolor (mi);
            (void) XDrawPoint (display, window,
                               pp->stippledGC,
                               x * pp->xs + pp->xb, y * pp->ys + pp->yb);
        }

        return;
    }

    switch (pp->level[y * LEVWIDTH + x]) {
    case ' ':
    case '=':
        break;

    case '.':
        setdotcolor (mi);
        if (pp->xs < 8 || pp->ys < 8) {
            (void) XDrawPoint (display, window,
                               pp->stippledGC,
                               x * pp->xs + pp->xb +
                               pp->xs / 2, y * pp->ys + pp->yb + pp->ys / 2);
            break;
        }

        draw_regular_dot (mi, pp, x, y);
        break;
        /* What we will probably want to do here is have the pp->level store a 'o' for
         * the bonus dots. The we can use the drawing routine above just with a bigger
         * radius.
         */
    case 'o':
        draw_bonus_dot (mi, pp, x, y);
        break;


    case '-':
        setwallcolor (mi);
        (void) XDrawLine (display, window, pp->stippledGC,
                          (pp->xs * x) + pp->xb,
                          (pp->ys * y) + (pp->ys / 2) + pp->yb,
                          (pp->xs * (x + 1)) + pp->xb,
                          (pp->ys * y) + (pp->ys / 2) + pp->yb);
        break;

    case '|':
        setwallcolor (mi);
        (void) XDrawLine (display, window, pp->stippledGC,
                          (pp->xs * x) + (pp->xs / 2) + pp->xb,
                          (pp->ys * y) + pp->yb,
                          (pp->xs * x) + (pp->xs / 2) + pp->xb,
                          (pp->ys * (y + 1)) + pp->yb);
        break;

    case '_':
        setwallcolor (mi);
        (void) XDrawArc (display, window, pp->stippledGC,
                         (pp->xs * x) - (pp->ys / 2) + pp->xb + dx,
                         (pp->ys * y) + (pp->ys / 2) + pp->yb,
                         pp->xs, pp->ys, 0 * 64, 90 * 64);
        break;

    case ',':
        setwallcolor (mi);
        (void) XDrawArc (display, window, pp->stippledGC,
                         (pp->xs * x) + (pp->ys / 2) + pp->xb,
                         (pp->ys * y) + (pp->ys / 2) + pp->yb,
                         pp->xs, pp->ys, 90 * 64, 90 * 64);
        break;

    case '`':
        setwallcolor (mi);
        (void) XDrawArc (display, window, pp->stippledGC,
                         (pp->xs * x) + (pp->ys / 2) + pp->xb,
                         (pp->ys * y) - (pp->ys / 2) + pp->yb + dy,
                         pp->xs, pp->ys, 180 * 64, 90 * 64);
        break;

    case '\'':
        setwallcolor (mi);
        (void) XDrawArc (display, window, pp->stippledGC,
                         (pp->xs * x) - (pp->ys / 2) + pp->xb + dx,
                         (pp->ys * y) - (pp->ys / 2) + pp->yb + dy,
                         pp->xs, pp->ys, 270 * 64, 90 * 64);
        break;

    }
}

/* Draws a complete level. */
static void
drawlevel (ModeInfo * mi)
{
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    unsigned int x, y;

    for (y = 0; y < LEVHEIGHT; y++)
        for (x = 0; x < LEVWIDTH; x++)
            drawlevelblock (mi, pp, x, y);
}

/* There is some overlap so it can be made more efficient */
#define ERASE_IMAGE(d,w,g,x,y,xl,yl,xs,ys) \
 if (yl<y) \
  (y<(yl+ys))?XFillRectangle(d,w,g,xl,yl,xs,y-(yl)): \
  XFillRectangle(d,w,g,xl,yl,xs,ys); \
 else if (yl>y) \
  (y>(yl-(ys)))?XFillRectangle(d,w,g,xl,y+ys,xs,yl-(y)): \
  XFillRectangle(d,w,g,xl,yl,xs,ys); \
 if (xl<x) \
  (x<(xl+xs))?XFillRectangle(d,w,g,xl,yl,x-(xl),ys): \
  XFillRectangle(d,w,g,xl,yl,xs,ys); \
 else if (xl>x) \
  (x>(xl-(xs)))?XFillRectangle(d,w,g,x+xs,yl,xl-(x),ys): \
  XFillRectangle(d,w,g,xl,yl,xs,ys)


/* Draws the pacman sprite, removing the previous location. */

static void
draw_pacman_sprite (ModeInfo * mi)
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    unsigned int dir = 0;
    int old_mask_dir = 0;
    int old_mask_mouth = 0;
    Pixmap old_mask, new_mask;
    Pixmap pacman;

#define MAX_MOUTH_DELAY 2
#define MAX_DEATH_DELAY 20

    if (pp->pacman.aistate == ps_dieing){
        pp->pacman.cf = pp->pacman.oldcf;
        pp->pacman.rf = pp->pacman.oldrf;
    }
    else {
        pp->pacman.cf = pp->pacman.col * pp->xs + pp->pacman.delta.x *
            pp->pacman.cfactor + pp->xb + pp->spritedx;
        pp->pacman.rf = pp->pacman.row * pp->ys + pp->pacman.delta.y *
            pp->pacman.rfactor + pp->yb + pp->spritedy;
    }

    dir = (ABS (pp->pacman.cfactor) * (2 - pp->pacman.cfactor) +
           ABS (pp->pacman.rfactor) * (1 + pp->pacman.rfactor)) % 4;

    if (pp->pm_mouth_delay == MAX_MOUTH_DELAY) {
        if (pp->pm_mouth == (MAXMOUTH - 1) || pp->pm_mouth == 0) {
            pp->pm_open_mouth = !pp->pm_open_mouth;
        }
        pp->pm_open_mouth ? pp->pm_mouth++ : pp->pm_mouth--;
        pp->pm_mouth_delay = 0;
    }
    else {
        pp->pm_mouth_delay++;
    }
    
    if (pp->pacman.aistate == ps_dieing){
        if (pp->pm_death_frame >= PAC_DEATH_FRAMES) {
            pp->pacman.aistate = ps_eating;
            pp->pm_death_frame = 0;
            pp->pm_death_delay = 0;
            reset_level (mi, 0, False);
            return;
        }
        else {
            old_mask   = pp->pacmanMask[0][0];
            new_mask   = pp->pacmanMask[0][0];
            pacman     = pp->pacman_ds[pp->pm_death_frame];
            if (pp->pm_death_delay == MAX_DEATH_DELAY){
                pp->pm_death_frame++;
                pp->pm_death_delay = 0;
            }
            else{
                pp->pm_death_delay++;
            }
        }
    }
    else{
        old_mask = pp->pacmanMask[old_mask_dir][old_mask_mouth];
        new_mask = pp->pacmanMask[dir][pp->pm_mouth];
        pacman   = pp->pacmanPixmap[dir][pp->pm_mouth];
    }

    XSetForeground (display, pp->stippledGC, MI_BLACK_PIXEL (mi));

    XSetClipMask (display, pp->stippledGC, old_mask);
                  
    XSetClipOrigin (display, pp->stippledGC, pp->pacman.oldcf,
                    pp->pacman.oldrf);
    XFillRectangle (display, window, pp->stippledGC, pp->pacman.oldcf,
                    pp->pacman.oldrf, pp->spritexs, pp->spriteys);
    XSetClipMask (display, pp->stippledGC, new_mask);
    XSetClipOrigin (display, pp->stippledGC, pp->pacman.cf, pp->pacman.rf);
    XCopyArea (display, pacman, window,
               pp->stippledGC, 0, 0, pp->spritexs, pp->spriteys,
               pp->pacman.cf, pp->pacman.rf);
    XSetClipMask (display, pp->stippledGC, None);
    if (pp->pacman.aistate != ps_dieing){
        pp->pacman.oldcf = pp->pacman.cf;
        pp->pacman.oldrf = pp->pacman.rf;
    }
}

#if 0
static void
draw_ghost_position (ModeInfo * mi, ghoststruct * ghost)
{
    draw_position (mi, ghost->oldcf, ghost->oldrf, MI_BLACK_PIXEL (mi));
    draw_position (mi, ghost->cf, ghost->rf, 0x00ff00);
}
#endif
#if 0
static void
draw_ghost_ndirs ( ModeInfo *mi, ghoststruct * ghost)
{
    draw_number (mi, ghost->oldcf, ghost->oldrf, ghost->oldndirs, MI_BLACK_PIXEL (mi));
    ghost->oldndirs = ghost->ndirs;
    draw_number (mi, ghost->cf, ghost->rf, ghost->ndirs, 0x00ff00);
}

#endif

static void
draw_ghost_sprite (ModeInfo * mi, const unsigned ghost)
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
#define MAX_WAG_COUNT 50
    unsigned int dir = 0;
    unsigned int fs  = 0; /*flash scared*/
    Pixmap g_pix; /*ghost pixmap*/

    
    dir = (ABS (pp->ghosts[ghost].cfactor) * (2 - pp->ghosts[ghost].cfactor) +
           ABS (pp->ghosts[ghost].rfactor) * (1 + pp->ghosts[ghost].rfactor)) % 4;
                                             

    fs = pp->ghosts[ghost].flash_scared;
    assert (fs == 0 || fs == 1);

    /* Choose the pixmap */
    switch (pp->ghosts[ghost].aistate){
    case hiding:
        g_pix = pp->s_ghostPixmap[fs][pp->gh_wag];
        break;
    case goingin:
        g_pix = pp->ghostEyes[dir];
#if 1
        {
            int i = 0;
            while ( i < pp->ghosts[ghost].trace_idx ){
                XFillRectangle (display,
                                window,
                                pp->stippledGC,
                                pp->ghosts[ghost].trace[i].vx,
                                pp->ghosts[ghost].trace[i].vy, 
                                pp->spritexs, pp->spriteys);

                i++;
            }
        }
#endif
                
        break;
    default:
        g_pix = pp->ghostPixmap[ghost][dir][pp->gh_wag];
    }

    pp->ghosts[ghost].cf =
        pp->ghosts[ghost].col * pp->xs + pp->ghosts[ghost].delta.x *
        pp->ghosts[ghost].cfactor + pp->xb + pp->spritedx;
    pp->ghosts[ghost].rf =
        pp->ghosts[ghost].row * pp->ys + pp->ghosts[ghost].delta.y *
        pp->ghosts[ghost].rfactor + pp->yb + pp->spritedy;

    XSetForeground (display, pp->stippledGC, MI_BLACK_PIXEL (mi));

    XSetClipMask (display, pp->stippledGC, pp->ghostMask);
    XSetClipOrigin (display, pp->stippledGC,
                    pp->ghosts[ghost].oldcf, pp->ghosts[ghost].oldrf);
    XFillRectangle (display,
                    window,
                    pp->stippledGC,
                    pp->ghosts[ghost].oldcf,
                    pp->ghosts[ghost].oldrf, pp->spritexs, pp->spriteys);

    
    if (pp->pacman.aistate != ps_dieing) {
        drawlevelblock (mi, pp,
                        (unsigned int) pp->ghosts[ghost].col,
                        (unsigned int) pp->ghosts[ghost].row);



        XSetClipOrigin (display, pp->stippledGC,
                        pp->ghosts[ghost].cf, pp->ghosts[ghost].rf);

        XCopyArea (display, g_pix, window, pp->stippledGC, 0, 0,
                   pp->spritexs, pp->spriteys, pp->ghosts[ghost].cf,
                   pp->ghosts[ghost].rf);
    }
    XSetClipMask (display, pp->stippledGC, None);

#if 0
    draw_ghost_position (mi, &(pp->ghosts[ghost]));
#endif

#if 0
    draw_ghost_ndirs ( mi, &(pp->ghosts[ghost]));
#endif
    
    if (pp->pacman.aistate != ps_dieing) {
        pp->ghosts[ghost].oldcf = pp->ghosts[ghost].cf;
        pp->ghosts[ghost].oldrf = pp->ghosts[ghost].rf;
        if (pp->gh_wag_count++ == MAX_WAG_COUNT) {
            pp->gh_wag = !pp->gh_wag;
            pp->gh_wag_count = 0;
        }
    }
}


static int
ghost_over (ModeInfo * mi, int x, int y)
{
    int ghost = 0;
    int ret = False;
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    dot_rc_to_pixel (mi, &x, &y);
    for (ghost = 0; ghost < pp->nghosts; ghost++) {
        if ((pp->ghosts[ghost].cf <= x
             && x <= pp->ghosts[ghost].cf + pp->spritexs)
            && (pp->ghosts[ghost].rf <= y
                && y <= pp->ghosts[ghost].rf + pp->spriteys)) {
            ret = True;
            break;
        }
    }
    return ret;
}


static void
flash_bonus_dots (ModeInfo * mi)
{
#define MAX_FLASH_COUNT 25
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    int i, x, y;
    for (i = 0; i < NUM_BONUS_DOTS; i++) {
        if (!pacman_bonus_dot_eaten (pp, i)) {
            pacman_bonus_dot_pos (pp, i, &x, &y);
            if (ghost_over (mi, x, y))
                continue;
            if (pp->bd_on)
                draw_bonus_dot (mi, pp, x, y);
            else
                clear_bonus_dot (mi, pp, x, y);
        }
    }
    if (pp->bd_flash_count-- == 0) {
        pp->bd_flash_count = MAX_FLASH_COUNT;
        pp->bd_on = !pp->bd_on;
    }
}

static unsigned int
ate_bonus_dot (ModeInfo * mi)
{
    /*Check pacman's position. If it is over a bonus dot and that dot
     *has not been eaten, then return true
     */
    unsigned int ret = 0;
    int idx = 0;
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    if (pacman_is_bonus_dot (pp, pp->pacman.col, pp->pacman.row, &idx)) {
        ret = !pacman_bonus_dot_eaten (pp, idx);
        pacman_eat_bonus_dot (pp, idx);
    }
    return ret;
}

static void
ghost_scared (ModeInfo * mi)
{
    unsigned int ghost;
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    for (ghost = 0; ghost < pp->nghosts; ghost++) {
        if (pp->ghosts[ghost].aistate == goingin || 
            pp->ghosts[ghost].aistate == goingout ||
            pp->ghosts[ghost].aistate == inbox ) continue;
        pp->ghosts[ghost].aistate = hiding;
        pp->ghosts[ghost].flash_scared = 0;
        if (pp->pacman.aistate != ps_dieing)
            pp->pacman.aistate = ps_chasing;
    }
}

static void
ghost_not_scared (ModeInfo * mi)
{
    unsigned int ghost;
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    for (ghost = 0; ghost < pp->nghosts; ghost++){
        if (pp->ghosts[ghost].aistate == goingin ||
            pp->ghosts[ghost].aistate == goingout ||
            pp->ghosts[ghost].aistate == inbox ) continue;
        pp->ghosts[ghost].aistate = chasing;
    }
    if (pp->pacman.aistate != ps_dieing)
        pp->pacman.aistate = ps_eating;
    
}

static void
ghost_flash_scared (ModeInfo * mi)
{
    unsigned int ghost;
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    for (ghost = 0; ghost < pp->nghosts; ghost++)
        pp->ghosts[ghost].flash_scared = !pp->ghosts[ghost].flash_scared;
}

/* Does all drawing of moving sprites in the level. */
static void
pacman_tick (ModeInfo * mi)
{
#define DEFAULT_SCARED_TIME 500
#define START_FLASH 200
#define FLASH_COUNT 25

    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    unsigned int ghost;
#if 0
    draw_grid (mi);
#endif
    for (ghost = 0; ghost < pp->nghosts; ghost++) {
        draw_ghost_sprite (mi, ghost);
#if 0
        print_ghost_stats (mi, &(pp->ghosts[ghost]), ghost);
#endif
    }
#if 0    
    print_pac_stats (mi, &(pp->pacman));
#endif
    draw_pacman_sprite (mi);
    flash_bonus_dots (mi);
    if (ate_bonus_dot (mi)) {
        pp->ghost_scared_timer = (random () % 100) + DEFAULT_SCARED_TIME;
        ghost_scared (mi);
    }

    if (pp->ghost_scared_timer > 0) {
        if (--pp->ghost_scared_timer == 0)
            ghost_not_scared (mi);
        else if (pp->ghost_scared_timer <= START_FLASH) {
            if (pp->flash_timer <= 0) {
                pp->flash_timer = FLASH_COUNT;
                ghost_flash_scared (mi);
            }
            pp->flash_timer--;
        }
    }

    /*
      We don't want to miss the last death sequence. So if pacman has died three times
      we wait for his state to change from dieing to something else before we repopulate
      the level. If pacman ate all of the dots then we just repopulate.
    */

    if (pp->dotsleft == 0 )
        repopulate (mi);
    else if (pp->pacman.deaths >= 3){
        if (pp->old_pac_state == ps_dieing && pp->pacman.aistate != ps_dieing)
            repopulate (mi);
    }

    pp->old_pac_state = pp->pacman.aistate;
}


/* CODE TO LOAD AND SCALE THE PIXMAPS
 */

/*  Grabbed the scaling routine off of usenet. 
 *  Changed it so that the information specific 
 *  to the source pixmap does not have to be a parameter.
 *
 *  There is probably a better way to scale pixmaps.
 *  From: Chris Fiddyment (cxf@itd.dsto.gov.au)
 *  Subject: Scaling Pixmap Algorithm.
 *  Newsgroups: comp.graphics.algorithms
 *  Date: 1994-07-06 18:51:38 PST 
 *  -jeremy
 */

static Pixmap
scale_pixmap (Display ** dpy, GC gc, Pixmap source, int dwidth, int dheight)
{
    Pixmap temp, dest;
    int j, end;
    float i;
    float xscale, yscale;
    unsigned int swidth, sheight;
    Window window;
    int x, y;
    unsigned border_width_return, depth;
    XGetGeometry (*dpy, source, &window, &x, &y, &swidth, &sheight,
                  &border_width_return, &depth);

    xscale = (float) swidth / (float) dwidth;   /* Scaling factors */
    yscale = (float) sheight / (float) dheight;

    dest = XCreatePixmap (*dpy, window, dwidth, dheight, depth);
    if (!dest) {
        fprintf (stderr, "%s Could not scale image", progname);
    }
    temp = XCreatePixmap (*dpy, window, dwidth, sheight, depth);
    if (!temp) {
        fprintf (stderr, "%s Could not scale image", progname);
    }

    j = 0;
    end = dwidth * xscale;
    /* Scale width of source into temp pixmap */
    for (i = 0; i <= end; i += xscale)
        XCopyArea (*dpy, source, temp, gc, i, 0, 1, sheight, j++, 0);

    j = 0;
    end = dheight * yscale;
    /* Scale height of temp into dest pixmap */
    for (i = 0; i <= end; i += yscale)
        XCopyArea (*dpy, temp, dest, gc, 0, i, dwidth, 1, 0, j++);

    XFreePixmap (*dpy, temp);
    return (Pixmap) dest;
}

static Pixmap
subpixmap (Display *dpy, Window window, Pixmap src,
           int w, int h, int y, int depth)
{
    XGCValues gcv;
    Pixmap dest = XCreatePixmap (dpy, window, w, h, depth);
    GC gc = XCreateGC (dpy, src, 0, &gcv);
    XCopyArea (dpy, src, dest, gc, 0, y, w, h, 0, 0);
    XFreeGC (dpy, gc);
    return dest;
}


/* Load the ghost pixmaps and their mask. */
static void
load_pixmaps (Display ** dpy, Window window, pacmangamestruct ** ps)
{
    pacmangamestruct *pp = *ps;
    Display *display = *dpy;
    Pixmap sprites, sprites_mask;
    int i, j, k, m, sw, sh, srcy;
/*    int w = pp->spritexs;
    int h = pp->spriteys;*/
    GC gc = 0;
/*    Pixmap temp;*/
    XGCValues gcv;
    XWindowAttributes xgwa;

    XGetWindowAttributes (display, window, &xgwa);

    sprites = image_data_to_pixmap (display, window,
                                    pacman_png, sizeof(pacman_png), &sw, &sh,
                                    &sprites_mask);
    if (!sprites || !sprites_mask) abort();

    srcy = 0;

    gc = XCreateGC (display, sprites_mask, 0, &gcv);

    pp->ghostMask = subpixmap (display, window, sprites_mask,
                               sw, sw, srcy, 1);
    pp->ghostMask = scale_pixmap (&display, gc, pp->ghostMask,
                                  pp->spritexs, pp->spriteys);

    for (i = 0; i < 4; i++) {
        m = 0;
        for (j = 0; j < MAXGDIR; j++) {
            for (k = 0; k < MAXGWAG; k++) {
                pp->ghostPixmap[i][j][k] =
                    subpixmap (display, window, sprites, sw, sw, srcy,
                               xgwa.depth);
                pp->ghostPixmap[i][j][k] =
                    scale_pixmap (&display, pp->stippledGC,
                                  pp->ghostPixmap[i][j][k], pp->spritexs,
                                  pp->spriteys);
                m++;
                srcy += sw;
                if (srcy >= sh) abort();
            }
        }
    }

    /* load the scared ghost */
    m = 0;
    for (i = 0; i < MAXGFLASH; i++) {
        for (j = 0; j < MAXGWAG; j++) {
            pp->s_ghostPixmap[i][j] =
                subpixmap (display, window, sprites, sw, sw, srcy,
                           xgwa.depth);
            m++;
            pp->s_ghostPixmap[i][j] = scale_pixmap (&display, pp->stippledGC,
                                                    pp->s_ghostPixmap[i][j],
                                                    pp->spritexs,
                                                    pp->spriteys);
            srcy += sw;
            if (srcy >= sh) abort();
        }
    }

    /* load the ghost eyes */
    for (i = 0; i < MAXGDIR; i++) {
        pp->ghostEyes[i] =
            subpixmap (display, window, sprites, sw, sw, srcy, xgwa.depth);
        pp->ghostEyes[i] = scale_pixmap (&display, pp->stippledGC,
                                         pp->ghostEyes[i],
                                         pp->spritexs,
                                         pp->spriteys);
        srcy += sw;
        if (srcy >= sh) abort();
    }


    /* Load the pacman pixmaps and their mask. */

    m = 0;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < MAXMOUTH; j++) {
            pp->pacmanPixmap[i][j] =
                subpixmap (display, window, sprites, sw, sw, srcy, 
                           xgwa.depth);
            pp->pacmanMask[i][j] =
                subpixmap (display, window, sprites_mask, sw, sw, srcy, 1);
            m++;
            pp->pacmanPixmap[i][j] = scale_pixmap (&display, pp->stippledGC,
                                                   pp->pacmanPixmap[i][j],
                                                   pp->spritexs,
                                                   pp->spriteys);
            pp->pacmanMask[i][j] =
                scale_pixmap (&display, gc, pp->pacmanMask[i][j],
                              pp->spritexs, pp->spriteys);
            srcy += sw;
            if (srcy >= sh) abort();
        }
    }
    
    /* Load pacman death sequence */
    for ( i = 0; i < PAC_DEATH_FRAMES; i++ ){
        if (srcy > sh - sw) abort();
        pp->pacman_ds[i] = 
            subpixmap (display, window, sprites, sw, sw, srcy, xgwa.depth);
        pp->pacman_ds_mask[i] =
            subpixmap (display, window, sprites_mask, sw, sw, srcy, 1);
        
        pp->pacman_ds[i] = scale_pixmap ( &display, pp->stippledGC,
                                          pp->pacman_ds[i],
                                          pp->spritexs,
                                          pp->spriteys);
        pp->pacman_ds_mask[i] = 
            scale_pixmap (&display, gc, pp->pacman_ds_mask[i],
                          pp->spritexs, pp->spriteys);
        srcy += sw;
    }
}


/* Hook function, sets state to initial position. */
ENTRYPOINT void
init_pacman (ModeInfo * mi)
{
    Display *display = MI_DISPLAY (mi);
    Window window = MI_WINDOW (mi);
    long size = MI_SIZE (mi);
    pacmangamestruct *pp;
    XGCValues gcv;
    int i, j, k;

    MI_INIT (mi, pacman_games);
    pp = &pacman_games[MI_SCREEN (mi)];

    pp->width = (unsigned short) MI_WIDTH (mi);
    pp->height = (unsigned short) MI_HEIGHT (mi);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < MAXGDIR; j++) {
            for (k = 0; k < MAXGWAG; k++) {
                if (pp->ghostPixmap[i][j][k] != None) {
                    XFreePixmap (display, pp->ghostPixmap[i][j][k]);
                    pp->ghostPixmap[i][j][k] = None;
                    pp->graphics_format = 0 /*IS_NONE */ ;
                }
            }
        }
    }

    for (i = 0; i < MAXGFLASH; i++) {
        for (j = 0; j < MAXGWAG; j++) {
            if (pp->s_ghostPixmap[i][j] != None) {
                XFreePixmap (display, pp->s_ghostPixmap[i][j]);
                pp->s_ghostPixmap[i][j] = None;
            }
        }
    }

    if (size == 0 ||
        MINGRIDSIZE * size > (int) pp->width ||
        MINGRIDSIZE * size > (int) pp->height) {
        double scale = MIN (pp->width / LEVWIDTH, pp->height / LEVHEIGHT);

        if (pp->width > pp->height * 5 ||  /* weird window aspect ratio */
            pp->height > pp->width * 5)
            scale = 0.8 * (pp->width / pp->height
                           ? pp->width / (double) pp->height
                           : pp->height / (double) pp->width);
        pp->ys = MAX (scale, 1);
        pp->xs = pp->ys;
    }
    else {
        if (size < -MINSIZE)
            pp->ys = (short) (NRAND (MIN (-size, MAX (MINSIZE,
                                                      MIN (pp->width,
                                                           pp->height) /
                                                      MINGRIDSIZE))
                                     - MINSIZE + 1) + MINSIZE);
        else if (size < MINSIZE)
            pp->ys = MINSIZE;
        else
            pp->ys = (short) (MIN (size,
                                   MAX (MINSIZE, MIN (pp->width, pp->height) /
                                        MINGRIDSIZE)));
        pp->xs = pp->ys;
    }


    pp->wallwidth = (unsigned int) (pp->xs + pp->ys) >> 4;
    if (pp->wallwidth < 1)
        pp->wallwidth = 1;
    pp->incx = (pp->xs >> 3) + 1;
    pp->incy = (pp->ys >> 3) + 1;
    pp->ncols = (unsigned short) MAX (LEVWIDTH, 2);
    pp->nrows = (unsigned short) MAX (LEVHEIGHT, 2);
    pp->xb = (pp->width - pp->ncols * pp->xs) >> 1;
    pp->yb = (pp->height - pp->nrows * pp->ys) >> 1;
    pp->spritexs = MAX (pp->xs + (pp->xs >> 1) - 1, 1);
    pp->spriteys = MAX (pp->ys + (pp->ys >> 1) - 1, 1);
    pp->spritedx = (pp->xs - pp->spritexs) >> 1;
    pp->spritedy = (pp->ys - pp->spriteys) >> 1;
    pp->old_pac_state = ps_chasing;

    if (!pp->stippledGC) {
        gcv.foreground = MI_BLACK_PIXEL (mi);
        gcv.background = MI_BLACK_PIXEL (mi);
        if ((pp->stippledGC = XCreateGC (display, window,
                                         GCForeground | GCBackground,
                                         &gcv)) == None) {
            free_pacman (mi);
            return;
        }
    }

#ifdef HAVE_JWXYZ
    jwxyz_XSetAntiAliasing (display, pp->stippledGC, False);
#endif

    load_pixmaps (&display, window, &pp);

    pp->pacman.lastbox = START;
    pp->pacman.mouthdirection = 1;
    pp->pacman.nextcol = NOWHERE;
    pp->pacman.nextrow = NOWHERE;

    if (pp->ghosts != NULL) {
        free (pp->ghosts);
        pp->ghosts = (ghoststruct *) NULL;
    }
    pp->nghosts = GHOSTS;

    if (!pp->ghosts)
        if ((pp->ghosts = (ghoststruct *) calloc ((size_t) pp->nghosts,
                                                  sizeof (ghoststruct))) ==
            NULL) {
            free_pacman (mi);
            return;
        }

    pp->pacman.mouthstage = MAXMOUTH - 1;

    XClearWindow (MI_DISPLAY(mi), MI_WINDOW(mi));
    repopulate (mi);
}

/* Callback function called for each tick.  This is the complete machinery of
   everything that moves. */
ENTRYPOINT void
draw_pacman (ModeInfo * mi)
{
    unsigned int g;
    pacmangamestruct *pp;

    if (pacman_games == NULL)
        return;
    pp = &pacman_games[MI_SCREEN (mi)];
    if (pp->ghosts == NULL)
        return;

    pp->pacman.err.x = (pp->pacman.err.x + 1) % pp->pacman.speed;
    pp->pacman.err.y = (pp->pacman.err.y + 1) % pp->pacman.speed;
    pp->pacman.delta.x += pp->pacman.err.x != 0 ? pp->incx : 0;
    pp->pacman.delta.y += pp->pacman.err.y != 0 ? pp->incy : 0;

    if (pp->pacman.delta.x >= pp->xs && pp->pacman.delta.y >= pp->ys) {
        pacman_update (mi, pp, &(pp->pacman));
        check_death (mi, pp);
        pp->pacman.delta.x = pp->incx;
        pp->pacman.delta.y = pp->incy;
    }

    if (pp->pacman.delta.x > pp->xs + pp->incx)
        pp->pacman.delta.x = pp->xs + pp->incx;
    if (pp->pacman.delta.y > pp->ys + pp->incy)
        pp->pacman.delta.y = pp->ys + pp->incy;

    for (g = 0; g < pp->nghosts; g++) {
        pp->ghosts[g].err.x = (pp->ghosts[g].err.x + 1) % pp->ghosts[g].speed;
        pp->ghosts[g].err.y = (pp->ghosts[g].err.y + 1) % pp->ghosts[g].speed;
        pp->ghosts[g].delta.x += pp->ghosts[g].err.x != 0 ? pp->incx : 0;
        pp->ghosts[g].delta.y += pp->ghosts[g].err.y != 0 ? pp->incy : 0; 
        
        if (pp->ghosts[g].delta.x >= pp->xs &&
            pp->ghosts[g].delta.y >= pp->ys) {
            pacman_ghost_update (pp, &(pp->ghosts[g]));
            pp->ghosts[g].delta.x = pp->incx;
            pp->ghosts[g].delta.y = pp->incy;
        }

        if (pp->ghosts[g].delta.x > pp->xs + pp->incx)
            pp->ghosts[g].delta.x = pp->xs + pp->incx;
        if (pp->ghosts[g].delta.y > pp->ys + pp->incy)
            pp->ghosts[g].delta.y = pp->ys + pp->incy;
    }
    pacman_tick (mi);
}

#ifndef STANDALONE
/* Refresh current level. */
ENTRYPOINT void
refresh_pacman (ModeInfo * mi)
{
    drawlevel (mi);
    pacman_tick (mi);
}
#endif

ENTRYPOINT void
reshape_pacman(ModeInfo * mi, int width, int height)
{
    pacmangamestruct *pp = &pacman_games[MI_SCREEN (mi)];
    pp->width  = width;
    pp->height = height;
    pp->xb = (pp->width  - pp->ncols * pp->xs) >> 1;
    pp->yb = (pp->height - pp->nrows * pp->ys) >> 1;
    XClearWindow (MI_DISPLAY(mi), MI_WINDOW(mi));
    /* repopulate (mi); */
    drawlevel (mi);
}

#ifndef STANDALONE
/* Callback to change level. */
ENTRYPOINT void
change_pacman (ModeInfo * mi)
{
    XClearWindow (MI_DISPLAY(mi), MI_WINDOW(mi));
    repopulate (mi);
}
#endif /* !STANDALONE */


XSCREENSAVER_MODULE ("Pacman", pacman)

#endif /* MODE_pacman */
