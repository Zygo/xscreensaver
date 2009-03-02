/* -*- Mode: C; tab-width: 4 -*- */
/* pacman --- Mr. Pacman and his ghost friends */

/*static const char sccsid[] = "@(#)pacman.c	5.00 2000/11/01 xlockmore";*/

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
 * 15-Aug-2004: Added support for pixmap pacman. Jeremy English jenglish@myself.com
 * 11-Aug-2004: Added support for pixmap ghost. jenglish@myself.com
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
   1. add "bonus" dots
   2. think of a better level generation algorithm
*/

#define DEF_TRACKMOUSE "False"

#ifdef STANDALONE
#	define MODE_pacman
#	define PROGCLASS "Pacman"
#	define HACK_INIT init_pacman
#	define HACK_DRAW draw_pacman
#	define pacman_opts xlockmore_opts
#	define DEFAULTS "*delay: 10000 \n" \
		 			"*size: 0 \n" \
 					"*ncolors: 6 \n" \
					"*trackmouse: " DEF_TRACKMOUSE "\n"
#	define UNIFORM_COLORS
#	define BRIGHT_COLORS

#	include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#	include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

#ifdef MODE_pacman

#include "pacman.h"
#include "pacman_ai.h"
#include "pacman_level.h"

#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
#define USE_PIXMAP
#include "xpm-pixmap.h"
#else
#if defined(USE_PIXMAP)
#undef USE_PIXMAP
#endif
#endif

#if defined(USE_PIXMAP)
# include "images/pacman/ghost-u1.xpm"
# include "images/pacman/ghost-u2.xpm"
# include "images/pacman/ghost-r1.xpm"
# include "images/pacman/ghost-r2.xpm"
# include "images/pacman/ghost-l1.xpm"
# include "images/pacman/ghost-l2.xpm"
# include "images/pacman/ghost-d1.xpm"
# include "images/pacman/ghost-d2.xpm"
# include "images/pacman/ghost-mask.xpm" /* Used to clean up the dust left by wag. */
# include "images/pacman/pacman-u1.xpm"
# include "images/pacman/pacman-u2.xpm"
# include "images/pacman/pacman-r1.xpm"
# include "images/pacman/pacman-r2.xpm"
# include "images/pacman/pacman-l1.xpm"
# include "images/pacman/pacman-l2.xpm"
# include "images/pacman/pacman-d1.xpm"
# include "images/pacman/pacman-d2.xpm"
# include "images/pacman/pacman-0.xpm"
#endif

#ifdef DISABLE_INTERACTIVE
ModeSpecOpt pacman_opts = {
	0, 
	(XrmOptionDescRec *) NULL, 
	0, 
	(argtype *) NULL, 
	(OptionStruct *) NULL
};
#else
static XrmOptionDescRec opts[] =
{
	{"-trackmouse", ".pacman.trackmouse", XrmoptionNoArg, "on"},
	{"+trackmouse", ".pacman.trackmouse", XrmoptionNoArg, "off"}
};

static argtype vars[] =
{
	{&trackmouse, "trackmouse", "TrackMouse", DEF_TRACKMOUSE, t_Bool}
};

static OptionStruct desc[] =
{
	{"-/+trackmouse", "turn on/off the tracking of the mouse"}
};

ModeSpecOpt pacman_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};
#endif

#ifdef USE_MODULES
ModStruct   pacman_description = { 
	"pacman", 		/* *cmdline_arg; */
	"init_pacman", 		/* *init_name; */
	"draw_pacman", 		/* *callback_name; */
	"release_pacman",	/* *release_name; */
 	"refresh_pacman", 	/* *refresh_name; */
	"change_pacman", 	/* *change_name; */
	(char *) NULL, 		/* *unused_name; */
	&pacman_opts,		/* *msopts */
 	10000, 4, 1, 0, 64, 1.0, "", "Shows Pacman(tm)", 0, NULL
};

#endif

Bool trackmouse;
pacmangamestruct *pacmangames = (pacmangamestruct *) NULL;


static void repopulate(ModeInfo * mi);
static void drawlevel(ModeInfo * mi);


static void
free_pacman(Display *display, pacmangamestruct *pp)
{
	int         dir, mouth, i, j, k;

	if (pp->ghosts != NULL) {
		free(pp->ghosts);
		pp->ghosts = (ghoststruct *) NULL;
	}
	if (pp->stippledGC != None) {
	    XFreeGC(display, pp->stippledGC);
		pp->stippledGC = None;
	}
	for (i = 0; i < 4; i++){
	  for (j = 0; j < MAXGDIR; j++){
		for (k = 0; k < MAXGWAG; k++){
		  if (pp->ghostPixmap[i][j][k] != None) {
			XFreePixmap(display, pp->ghostPixmap[i][j][k]);
			pp->ghostPixmap[i][j][k] = None;
		  }
		}
	  }
	}
	for (dir = 0; dir < 4; dir++)
		for (mouth = 0; mouth < MAXMOUTH; mouth++)
			if (pp->pacmanPixmap[dir][mouth] != None) {
				XFreePixmap(display, 
					pp->pacmanPixmap[dir][mouth]);
				pp->pacmanPixmap[dir][mouth] = None;
			}
}


/* Checks for death of any ghosts/pacman and updates.  It also makes a new
   level if all ghosts are dead or all dots are eaten. */
static void
check_death(ModeInfo * mi, pacmangamestruct *pp)
{
	Display *display = 	MI_DISPLAY(mi);
	Window	 window	 = 	MI_WINDOW(mi);
	unsigned int	ghost;
	int	alldead;

	alldead = 1;
	for (ghost = 0; ghost < pp->nghosts; ghost++) {
		if (pp->ghosts[ghost].dead == True)
			continue;

		if ((pp->ghosts[ghost].nextrow == NOWHERE &&
		     pp->ghosts[ghost].nextcol == NOWHERE)) {
			alldead = 0;
			continue;
		}

		if (((pp->ghosts[ghost].nextrow == pp->pacman.nextrow) &&
		(pp->ghosts[ghost].nextcol == pp->pacman.nextcol)) ||
		    ((pp->ghosts[ghost].nextrow == pp->pacman.row) &&
		     (pp->ghosts[ghost].nextcol == pp->pacman.col) &&
		     (pp->ghosts[ghost].row == pp->pacman.nextrow) &&
		     (pp->ghosts[ghost].col == pp->pacman.nextcol))) {
			pp->ghosts[ghost].dead = 1;
			XSetForeground(display, 
					     pp->stippledGC, 
					     MI_BLACK_PIXEL(mi));
			XFillRectangle(display, window, 
					    pp->stippledGC,
					    pp->ghosts[ghost].cf, 
					    pp->ghosts[ghost].rf, 
					    pp->spritexs, 
					    pp->spriteys);

		} else
			alldead = 0;
	}

	if (alldead == 1 || pp->dotsleft == 0)
		repopulate(mi);
}

/* Resets state of ghosts + pacman.  Creates a new level, draws that level. */
static void
repopulate(ModeInfo * mi)
{
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];
	unsigned int ghost;
	int i = createnewlevel(mi);

	MI_CLEARWINDOW(mi);
	drawlevel(mi);

	pp->gamestate = GHOST_DANGER;

	pp->pacman.row = (LEVHEIGHT + JAILHEIGHT)/2 - i;
	pp->pacman.col = (LEVWIDTH/2);
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
	pac_clear_trace(&(pp->pacman));

	for (ghost = 0; ghost < pp->nghosts; ghost++) {
		pp->ghosts[ghost].col = (LEVWIDTH/2);
		pp->ghosts[ghost].row = (LEVHEIGHT/2);
		pp->ghosts[ghost].nextcol = NOWHERE;
		pp->ghosts[ghost].nextrow = NOWHERE;
		pp->ghosts[ghost].dead = 0;
		pp->ghosts[ghost].lastbox = START;
		pp->ghosts[ghost].cf = NOWHERE;
		pp->ghosts[ghost].rf = NOWHERE;
		pp->ghosts[ghost].oldcf = NOWHERE;
		pp->ghosts[ghost].oldrf = NOWHERE;
		pp->ghosts[ghost].aistate = inbox;
		pp->ghosts[ghost].timeleft = ghost * 50;
		pp->ghosts[ghost].speed = 3;
		pp->ghosts[ghost].delta.x = 0;
		pp->ghosts[ghost].delta.y = 0;

		ghost_update(pp, &(pp->ghosts[ghost]));
	}
	check_death(mi, pp);
	pac_update(mi, pp, &(pp->pacman));
}

/* Sets the color to the color of a wall. */
static void 
setwallcolor(ModeInfo * mi) 
{
	Display    *display = MI_DISPLAY(mi);
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, pp->stippledGC, 
				MI_PIXEL(mi, BLUE));
	else
		XSetForeground(display, pp->stippledGC, 
				MI_WHITE_PIXEL(mi));
}

/* Sets the color to the color of a dot. */
static void 
setdotcolor(ModeInfo * mi) 
{
	Display    *display = MI_DISPLAY(mi);
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];

	XSetForeground(display, pp->stippledGC, 
			MI_WHITE_PIXEL(mi));
}

/* Draws a block in the level at the specified x and y locations. */
static void
drawlevelblock(ModeInfo * mi, pacmangamestruct *pp, 
		const unsigned x, const unsigned y)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int dx = 0, dy = 0;

	if (pp->xs % 2 == 1) dx = -1;
	if (pp->ys % 2 == 1) dy = -1;

	XSetFillStyle(display, pp->stippledGC, FillSolid);
	XSetLineAttributes(display, pp->stippledGC, pp->wallwidth,
			LineSolid, CapRound, JoinMiter);

	if (pp->xs < 2 || pp->ys < 2) {
		switch(pp->level[y*LEVWIDTH + x]) {
			case ' ':
			case '=':
				break;
			case '.':
				setdotcolor(mi);
				(void)XDrawPoint(display, window,
						 pp->stippledGC,
						 x * pp->xs + pp->xb, 
						 y * pp->ys + pp->yb);
				break;
			default:
				setwallcolor(mi);
				(void)XDrawPoint(display, window,
						 pp->stippledGC,
						 x * pp->xs + pp->xb, 
						 y * pp->ys + pp->yb);
		}

		return;
	}

	switch (pp->level[y*LEVWIDTH + x]) {
		case ' ':
		case '=':
			break;

		case '.':
			setdotcolor(mi);
			if (pp->xs < 8 || pp->ys < 8) {
				(void)XDrawPoint(display, window,
						 pp->stippledGC,
						 x * pp->xs + pp->xb +
						 pp->xs/2, 
						 y * pp->ys + pp->yb +
						 pp->ys/2);
				break;
			}

			(void)XDrawArc(display, window, pp->stippledGC,
				(pp->xs * x) + 
					(pp->xs / 2) - 
					(pp->xs > 32 ? (pp->xs / 16) : 1) +
					pp->xb,
				(pp->ys * y) + 
					(pp->ys / 2) -
					(pp->ys > 32 ? (pp->ys / 16) : 1) +
					pp->yb,
				(pp->xs > 32 ? (pp->xs / 32) : 1),
				(pp->ys > 32 ? (pp->ys / 32) : 1),
				0, 23040);
			break;

		case '-':
			setwallcolor(mi);
			(void)XDrawLine(display, window, pp->stippledGC,
				(pp->xs * x) + pp->xb,
				(pp->ys * y) + (pp->ys / 2) + pp->yb,
				(pp->xs * (x + 1)) + pp->xb,
				(pp->ys * y) + (pp->ys / 2) + pp->yb);
			break;

		case '|':
			setwallcolor(mi);
			(void)XDrawLine(display, window, pp->stippledGC,
				(pp->xs * x) + (pp->xs / 2) + pp->xb,
				(pp->ys * y) + pp->yb,
				(pp->xs * x) + (pp->xs / 2) + pp->xb,
				(pp->ys * (y + 1)) + pp->yb);
			break;

		case '_':
			setwallcolor(mi);
			(void)XDrawArc(display, window, pp->stippledGC,
				(pp->xs * x) - (pp->ys / 2) + pp->xb + dx,
				(pp->ys * y) + (pp->ys / 2) + pp->yb,
				pp->xs, pp->ys,
				0*64, 90*64);
			break;

		case ',':
			setwallcolor(mi);
			(void)XDrawArc(display, window, pp->stippledGC,
				(pp->xs * x) + (pp->ys / 2) + pp->xb,
				(pp->ys * y) + (pp->ys / 2) + pp->yb,
				pp->xs, pp->ys,
				90*64, 90*64);
			break;

		case '`':
			setwallcolor(mi);
			(void)XDrawArc(display, window, pp->stippledGC,
				(pp->xs * x) + (pp->ys / 2) + pp->xb,
				(pp->ys * y) - (pp->ys / 2) + pp->yb + dy,
				pp->xs, pp->ys,
				180*64, 90*64);
			break;

		case '\'':
			setwallcolor(mi);
			(void)XDrawArc(display, window, pp->stippledGC,
				(pp->xs * x) - (pp->ys / 2) + pp->xb + dx,
				(pp->ys * y) - (pp->ys / 2) + pp->yb + dy,
				pp->xs, pp->ys,
				270*64, 90*64);
			break;

	}
}

/* Draws a complete level. */
static void
drawlevel(ModeInfo * mi)
{ 
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];
	unsigned int x, y;

	for (y = 0; y < LEVHEIGHT; y++)
		for (x = 0; x < LEVWIDTH; x++)
			drawlevelblock(mi, pp, x, y);
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
#if defined(USE_PIXMAP)

static void
draw_pacman_sprite(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];
	unsigned int dir = 0;
	int old_mask_dir = 0;
	int old_mask_mouth = 0;
	static int mouth = 0;
	static int mouth_delay = 0;
	static int open_mouth = 0;

#define MAX_MOUTH_DELAY 10

	pp->pacman.cf = pp->pacman.col * pp->xs + pp->pacman.delta.x * 
		pp->pacman.cfactor + pp->xb + pp->spritedx;
	pp->pacman.rf = pp->pacman.row * pp->ys + pp->pacman.delta.y * 
		pp->pacman.rfactor + pp->yb + pp->spritedy;

	dir = (ABS(pp->pacman.cfactor) * (2 - pp->pacman.cfactor) +
	       ABS(pp->pacman.rfactor) * (1 + pp->pacman.rfactor)) % 4;

	if (mouth_delay == MAX_MOUTH_DELAY){
	  if (mouth == (MAXMOUTH - 1)|| mouth == 0){
		open_mouth = !open_mouth;
	  }
	  open_mouth ? mouth++ : mouth--;
	  mouth_delay = 0;
	}else {
	  mouth_delay++;
	}

	XSetForeground(display, 
				   pp->stippledGC, 
				   MI_BLACK_PIXEL(mi));

	XSetClipMask(display, pp->stippledGC, pp->pacmanMask[old_mask_dir][old_mask_mouth]);
	XSetClipOrigin(display, pp->stippledGC, 
				   pp->pacman.oldcf, pp->pacman.oldrf);
	XFillRectangle(display, 
				   window, 
				   pp->stippledGC,
				   pp->pacman.oldcf, 
				   pp->pacman.oldrf, 
				   pp->spritexs, pp->spriteys);
	XSetClipMask(display, pp->stippledGC, pp->pacmanMask[dir][mouth]);
	XSetClipOrigin(display, pp->stippledGC, 
				   pp->pacman.cf, pp->pacman.rf);
	XCopyArea(display, pp->pacmanPixmap[dir][mouth], window, 
			  pp->stippledGC,0,0,pp->spritexs,pp->spriteys,
			  pp->pacman.cf, pp->pacman.rf);
	XSetClipMask(display, pp->stippledGC, None);
	pp->pacman.oldcf = pp->pacman.cf;
	pp->pacman.oldrf = pp->pacman.rf;
	old_mask_dir = dir;
	old_mask_mouth = mouth;
}



static void
draw_ghost_sprite(ModeInfo * mi, const unsigned ghost){
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];
	static int wag = 0;
#define MAX_WAG_COUNT 50
	static int wag_count = 0;
	unsigned int dir = 0;
	
	dir = (ABS(pp->ghosts[ghost].cfactor) * (2 - pp->ghosts[ghost].cfactor) +
	       ABS(pp->ghosts[ghost].rfactor) * (1 + pp->ghosts[ghost].rfactor)) % 4;

	pp->ghosts[ghost].cf = 
		pp->ghosts[ghost].col * pp->xs + pp->ghosts[ghost].delta.x * 
		pp->ghosts[ghost].cfactor + pp->xb + pp->spritedx;
	pp->ghosts[ghost].rf = 
		pp->ghosts[ghost].row * pp->ys + pp->ghosts[ghost].delta.y * 
		pp->ghosts[ghost].rfactor + pp->yb + pp->spritedy;
	XSetForeground(display, 
				   pp->stippledGC, 
				   MI_BLACK_PIXEL(mi));

	XSetClipMask(display, pp->stippledGC, pp->ghostMask);
	XSetClipOrigin(display, pp->stippledGC, 
				   pp->ghosts[ghost].oldcf, pp->ghosts[ghost].oldrf);
	XFillRectangle(display, 
				   window, 
				   pp->stippledGC,
				   pp->ghosts[ghost].oldcf, 
				   pp->ghosts[ghost].oldrf, 
				   pp->spritexs, pp->spriteys);
	drawlevelblock(mi, pp, 
				   (unsigned int)pp->ghosts[ghost].col, 
				   (unsigned int)pp->ghosts[ghost].row);
	XSetClipOrigin(display, pp->stippledGC, 
				   pp->ghosts[ghost].cf, pp->ghosts[ghost].rf);
	XCopyArea(display, pp->ghostPixmap[ghost][dir][wag], window, 
			  pp->stippledGC,0,0,pp->spritexs,pp->spriteys,
			  pp->ghosts[ghost].cf, pp->ghosts[ghost].rf);
	XSetClipMask(display, pp->stippledGC, None);
	pp->ghosts[ghost].oldcf = pp->ghosts[ghost].cf;
	pp->ghosts[ghost].oldrf = pp->ghosts[ghost].rf;
	if (wag_count++ == MAX_WAG_COUNT){
	  wag = !wag;
	  wag_count = 0;
	}
}

#else /* USE_PIXMAP */

/* Draws the pacman sprite, removing the previous location. */
static void
draw_pacman_sprite(ModeInfo * mi)
{
	Display *display = MI_DISPLAY(mi);
	Window window  = MI_WINDOW(mi);
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];
	unsigned int dir;

	pp->pacman.cf = pp->pacman.col * pp->xs + pp->pacman.delta.x * 
		pp->pacman.cfactor + pp->xb + pp->spritedx;
	pp->pacman.rf = pp->pacman.row * pp->ys + pp->pacman.delta.y * 
		pp->pacman.rfactor + pp->yb + pp->spritedy;

	dir = (ABS(pp->pacman.cfactor) * (2 - pp->pacman.cfactor) +
	       ABS(pp->pacman.rfactor) * (1 + pp->pacman.rfactor)) % 4;

	XSetForeground(display, pp->stippledGC, 
			     MI_BLACK_PIXEL(mi));
	if (pp->pacman.oldcf != NOWHERE && pp->pacman.oldrf != NOWHERE) {
#ifdef FLASH
		XFillRectangle(display, window, pp->stippledGC,
			     pp->pacman.oldcf, pp->pacman.oldrf, 
			     pp->spritexs, pp->spriteys);
#else
		ERASE_IMAGE(display, window, pp->stippledGC, 
			    pp->pacman.cf,    pp->pacman.rf,
			    pp->pacman.oldcf, pp->pacman.oldrf, 
			    pp->spritexs, pp->spriteys);
#endif
	}

	XSetTSOrigin(display, pp->stippledGC, 
			   pp->pacman.cf, pp->pacman.rf);
	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, pp->stippledGC, 
				     MI_PIXEL(mi, YELLOW));
	else
		XSetForeground(display, pp->stippledGC, 
				     MI_WHITE_PIXEL(mi));

	XSetStipple(display, pp->stippledGC,
		    pp->pacmanPixmap[dir][pp->pacman.mouthstage]);
#ifdef FLASH
	XSetFillStyle(display, pp->stippledGC, FillStippled);
#else
	XSetFillStyle(display, pp->stippledGC, 
			    FillOpaqueStippled);
#endif
	if (pp->xs < 2 || pp->ys < 2)
		XDrawPoint(display, window, pp->stippledGC,
				pp->pacman.cf, pp->pacman.rf);
	else
		XFillRectangle(display, window, pp->stippledGC,
			       pp->pacman.cf, pp->pacman.rf, 
			       pp->spritexs, pp->spriteys);
	pp->pacman.mouthstage += pp->pacman.mouthdirection;
	if ((pp->pacman.mouthstage >= MAXMOUTH) ||
	    (pp->pacman.mouthstage < 0)) {
		pp->pacman.mouthdirection *= -1;
		pp->pacman.mouthstage += pp->pacman.mouthdirection * 2;
	}
	pp->pacman.oldcf = pp->pacman.cf;
	pp->pacman.oldrf = pp->pacman.rf;
}

/* Draws a ghost sprite, removing the previous sprite and restores the level. */
static void
draw_ghost_sprite(ModeInfo * mi, const unsigned ghost) {
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];

	pp->ghosts[ghost].cf = 
		pp->ghosts[ghost].col * pp->xs + pp->ghosts[ghost].delta.x * 
		pp->ghosts[ghost].cfactor + pp->xb + pp->spritedx;
	pp->ghosts[ghost].rf = 
		pp->ghosts[ghost].row * pp->ys + pp->ghosts[ghost].delta.y * 
		pp->ghosts[ghost].rfactor + pp->yb + pp->spritedy;
	
	XSetForeground(display, 
			     pp->stippledGC, 
			     MI_BLACK_PIXEL(mi));
	XFillRectangle(display, 
				   window, 
				   pp->stippledGC,
				   pp->ghosts[ghost].cf, 
				   pp->ghosts[ghost].rf, 
				   pp->spritexs, pp->spriteys);

	if (pp->ghosts[ghost].oldcf != NOWHERE ||
		pp->ghosts[ghost].oldrf != NOWHERE) {
#ifdef FLASH
		XFillRectangle(display, window, 
				    pp->stippledGC, pp->ghosts[ghost].oldcf, 
				    pp->ghosts[ghost].oldrf, 
				    pp->spritexs, pp->spriteys);
#else
		ERASE_IMAGE(display, window, pp->stippledGC,
			pp->ghosts[ghost].cf, pp->ghosts[ghost].rf,
			pp->ghosts[ghost].oldcf, pp->ghosts[ghost].oldrf, 
			pp->spritexs, pp->spriteys);
#endif
	}

	drawlevelblock(mi, pp, 
		(unsigned int)pp->ghosts[ghost].col, 
		(unsigned int)pp->ghosts[ghost].row);

	XSetTSOrigin(display, pp->stippledGC,
		pp->ghosts[ghost].cf, pp->ghosts[ghost].rf);

	if (MI_NPIXELS(mi) > 2)
		XSetForeground(display, 
		     pp->stippledGC, 
		     MI_PIXEL(mi, GREEN));
	else
		XSetForeground(display, 
		     pp->stippledGC, 
		     MI_WHITE_PIXEL(mi));

	XSetStipple(display, pp->stippledGC, 
			  pp->ghostPixmap[0][0][0]);

#ifdef FLASH
	XSetFillStyle(display, 
			    pp->stippledGC, 
			    FillStippled);
#else
	XSetFillStyle(display, 
			    pp->stippledGC, 
			    FillOpaqueStippled);
#endif
	if (pp->xs < 2 || pp->ys < 2)
		XDrawPoint(display, window, pp->stippledGC,
				pp->ghosts[ghost].cf,
				pp->ghosts[ghost].rf);
	else
		XFillRectangle(display, 
				     window, 
				     pp->stippledGC,
				     pp->ghosts[ghost].cf, 
				     pp->ghosts[ghost].rf, 
				     pp->spritexs, pp->spriteys);

	pp->ghosts[ghost].oldcf = pp->ghosts[ghost].cf;
	pp->ghosts[ghost].oldrf = pp->ghosts[ghost].rf;
}
#endif /* USE_PIXMAP */

/* Does all drawing of moving sprites in the level. */
static void
pacman_tick(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	pacmangamestruct *pp = &pacmangames[MI_SCREEN(mi)];
	unsigned int ghost;

	for (ghost = 0; ghost < pp->nghosts; ghost++) {
		if (pp->ghosts[ghost].dead == True)
			continue;
		draw_ghost_sprite(mi, ghost);
	}

	draw_pacman_sprite(mi);

	(void)XFlush(display);
}

#if defined(USE_PIXMAP)
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
scale_pixmap( Display **dpy, GC gc, Pixmap source, int dwidth, int dheight)
{
   Pixmap temp,dest;
   int j,end;
   float i;
   float xscale, yscale;
   unsigned int swidth, sheight;
   Window window;
   int x, y;
   unsigned border_width_return, depth;
   XGetGeometry(*dpy, source, &window, &x, &y, &swidth, &sheight, &border_width_return, &depth);
   
   xscale = (float) swidth / (float) dwidth;         /* Scaling factors */
   yscale = (float) sheight / (float) dheight;
 
   dest = XCreatePixmap(*dpy,window,dwidth,dheight,depth);
   if (!dest){
	 fprintf(stderr, "%s Could not scale image", progname);
   }
   temp = XCreatePixmap(*dpy,window,dwidth,sheight,depth);
   if (!temp){
	 fprintf(stderr, "%s Could not scale image", progname);
   }
 
   j = 0;
   end = dwidth*xscale;
   /* Scale width of source into temp pixmap */
   for(i=0;i<end;i+=xscale)
      XCopyArea(*dpy,source,temp,gc,i,0,1,sheight,j++,0);
 
   j = 0;
   end = dheight*yscale;
   /* Scale height of temp into dest pixmap */
   for(i=0;i<end;i+=yscale)
      XCopyArea(*dpy,temp,dest,gc,0,i,dwidth,1,0,j++);
 
   XFreePixmap( *dpy, temp );
   return (Pixmap) dest;
}

static void
pacman_fail(char *s){
  fprintf (stderr, "%s: %s\n", progname, s);
  exit(1);
}  

/* Load the ghost pixmaps and their mask. */
static void
load_ghost_pixmaps(Display **dpy, Window window, pacmangamestruct **ps)
{
  pacmangamestruct *pp = *ps;
  Display *display = *dpy;
  static char *colors[]= {
	".	c #FF0000", /*Red*/
	".  c #00FFDE", /*Blue*/
	".  c #FFB847", /*Orange*/
	".  c #FFB8DE", /*Pink*/
  };
  
  static char **bits[] = {
	ghost_u1_xpm, ghost_u2_xpm, ghost_r1_xpm, ghost_r2_xpm,
	ghost_d1_xpm, ghost_d2_xpm, ghost_l1_xpm, ghost_l2_xpm
  };
  int i, j, k, m;
  int w = pp->spritexs;
  int h = pp->spriteys;
  GC gc = 0;
  Pixmap temp;
  
  for (i = 0; i < 4; i++){
	m = 0;
	for ( j = 0; j < MAXGDIR; j++){
	  for ( k = 0; k < MAXGWAG; k++){
		bits[m][2] = colors[i];
		pp->ghostPixmap[i][j][k] = xpm_data_to_pixmap (display, window, bits[m],
														   &w, &h, &pp->ghostMask);

		if (!pp->ghostPixmap[i][j][k]) pacman_fail("Cannot load ghost images");

		pp->ghostPixmap[i][j][k] = scale_pixmap(&display, pp->stippledGC, 
												pp->ghostPixmap[i][j][k], pp->spritexs, pp->spriteys);

		if (!pp->ghostPixmap[i][j][k]) pacman_fail("Cannot scale ghost images");
		m++;
	  }
	}
  }
  /* We really only need a single mask. This saves the headache of getting the bottom of the ghost
   * to clip just right. What we'll do is mask the top portion of the ghost, but the bottom of the
   * the ghost will be solid. I did this by setting the pixels between the fringe of their sheets
   * to black instead of none. -jeremy
   */
  temp = xpm_data_to_pixmap (display, window, ghost_mask_xpm,
							 &w, &h, &pp->ghostMask);

  if (!temp) pacman_fail("Cannot load temporary ghost image");
  
  temp = scale_pixmap(&display, pp->stippledGC, 
					  temp, pp->spritexs, pp->spriteys);

  if (!temp) pacman_fail("Cannot scale temporary ghost image");

  gc = XCreateGC(display, pp->ghostMask, 0, 0);

  pp->ghostMask = scale_pixmap(&display, gc, pp->ghostMask, 
							   pp->spritexs, pp->spriteys);
  XFreePixmap(display, temp);
}

/* Load the pacman pixmaps and their mask. */
static void
load_pacman_pixmaps(Display **dpy, Window window, pacmangamestruct **ps)
{
  pacmangamestruct *pp = *ps;
  Display *display = *dpy;

  static char **bits[] = {
	pacman_0_xpm,  pacman_u1_xpm, pacman_u2_xpm,
	pacman_0_xpm,  pacman_r1_xpm, pacman_r2_xpm,
	pacman_0_xpm,  pacman_d1_xpm, pacman_d2_xpm,
	pacman_0_xpm,  pacman_l1_xpm, pacman_l2_xpm
  };
  int i, j, m;
  int w = pp->spritexs;
  int h = pp->spriteys;
  GC gc = 0;

  m = 0;  
  for (i = 0; i < 4; i++){
	for ( j = 0; j < MAXMOUTH; j++){
	  pp->pacmanPixmap[i][j] = xpm_data_to_pixmap (display, window, bits[m++],
													 &w, &h, &pp->pacmanMask[i][j]);

	  if (!pp->pacmanPixmap[i][j]) pacman_fail("Cannot load pacman pixmap.");

	  pp->pacmanPixmap[i][j] = scale_pixmap(&display, pp->stippledGC, 
												pp->pacmanPixmap[i][j], pp->spritexs, pp->spriteys);

	  if (!pp->pacmanPixmap[i][j]) pacman_fail("Cannot scale pacman pixmap.");

	  if (!gc)
		gc = XCreateGC(display, pp->pacmanMask[i][j], 0, 0);

	  pp->pacmanMask[i][j] = scale_pixmap(&display, gc, pp->pacmanMask[i][j], 
								   pp->spritexs, pp->spriteys);	
	}
  }
}
#endif /* USE_PIXMAP */

/* Hook function, sets state to initial position. */
void
init_pacman(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         size = MI_SIZE(mi);
	pacmangamestruct *pp;
	XGCValues   gcv;
	int i, j, k;

#if (! defined( USE_PIXMAP ))
	GC          fg_gc, bg_gc;
	XPoint	    points[9];
	int dir, mouth;
#endif

	if (pacmangames == NULL) {
		if ((pacmangames = (pacmangamestruct *) 
					calloc((size_t)MI_NUM_SCREENS(mi),
					 sizeof (pacmangamestruct))) == NULL)
			return;
	}
	pp = &pacmangames[MI_SCREEN(mi)];

	pp->width = (unsigned short)MI_WIDTH(mi);
	pp->height = (unsigned short)MI_HEIGHT(mi); 
	for (i = 0; i < 4; i++){
	  for (j = 0; j < MAXGDIR; j++){
		for (k = 0; k < MAXGWAG; k++){
		  if (pp->ghostPixmap[i][j][k] != None) {
			XFreePixmap(display, pp->ghostPixmap[i][j][k]);
			pp->ghostPixmap[i][j][k] = None;
			pp->graphics_format = 0 /*IS_NONE*/;
		  }
		}
	  }
	}
	if (size == 0 ||
		MINGRIDSIZE * size > (int)pp->width ||
		MINGRIDSIZE * size > (int)pp->height) {

		pp->ys = pp->xs = MAX(MIN(pp->width/LEVWIDTH, 
				pp->height/LEVHEIGHT), 1);
	} else {
		if (size < -MINSIZE)
			pp->ys = (short)(NRAND( MIN( -size, MAX( MINSIZE, 
				   MIN( pp->width, pp->height) / MINGRIDSIZE)) 
					- MINSIZE + 1) + MINSIZE);
		else if (size < MINSIZE)
			pp->ys = MINSIZE;
		else
			pp->ys = (short)(MIN(size, 
				     MAX(MINSIZE, MIN(pp->width, pp->height) /
					  MINGRIDSIZE)));
		pp->xs = pp->ys;
	}

	pp->wallwidth = (unsigned int)(pp->xs + pp->ys) >> 4;
	if (pp->wallwidth < 1) pp->wallwidth = 1;
	pp->incx = (pp->xs >> 3) + 1;
	pp->incy = (pp->ys >> 3) + 1;
	pp->ncols = (unsigned short)MAX(LEVWIDTH, 2);
	pp->nrows = (unsigned short)MAX(LEVHEIGHT, 2);
	pp->xb = (pp->width - pp->ncols * pp->xs) >> 1;
	pp->yb = (pp->height - pp->nrows * pp->ys) >> 1;
	pp->spritexs = MAX(pp->xs + (pp->xs >> 1) - 1, 1);
	pp->spriteys = MAX(pp->ys + (pp->ys >> 1) - 1, 1);
	pp->spritedx = (pp->xs - pp->spritexs) >> 1;
	pp->spritedy = (pp->ys - pp->spriteys) >> 1;

	if (!pp->stippledGC) {
		gcv.foreground = MI_BLACK_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		if ((pp->stippledGC = XCreateGC(display, window,
				GCForeground | GCBackground, &gcv)) == None) {
			free_pacman(display, pp);
			return;
		}
	}

#if defined(USE_PIXMAP)
	load_ghost_pixmaps(&display,window,&pp);
	load_pacman_pixmaps(&display,window,&pp);
#else
	if ((pp->ghostPixmap[0][0][0] = XCreatePixmap(display, window,
		pp->spritexs, pp->spriteys, 1)) == None) {
		free_pacman(display, pp);
		return;
	}

	gcv.foreground = 0;
	gcv.background = 1;
	if ((bg_gc = XCreateGC(display, pp->ghostPixmap[0][0][0],
			GCForeground | GCBackground, &gcv)) == None) {
		free_pacman(display, pp);
		return;
	}

	gcv.foreground = 1;
	gcv.background = 0;
	if ((fg_gc = XCreateGC(display, pp->ghostPixmap[0][0][0],
			GCForeground | GCBackground, &gcv)) == None) {
		XFreeGC(display, bg_gc);
		free_pacman(display, pp);
		return;
	}

#define SETPOINT(p, xp, yp) p.x = xp; p.y = yp

	/* draw the triangles on the bottom (scalable) */
	SETPOINT(points[0], 1, 				pp->spriteys * 5 / 6);
	SETPOINT(points[1], pp->spritexs / 6,		pp->spriteys);
	SETPOINT(points[2], pp->spritexs / 3,		pp->spriteys * 5 / 6);
	SETPOINT(points[3], pp->spritexs / 2,		pp->spriteys);
	SETPOINT(points[4], pp->spritexs * 2 / 3,	pp->spriteys * 5 / 6);
	SETPOINT(points[5], pp->spritexs * 5 / 6,	pp->spriteys);
	SETPOINT(points[6], pp->spritexs,		pp->spriteys * 5 / 6);
	SETPOINT(points[7], pp->spritexs,		pp->spriteys / 2);
	SETPOINT(points[8], 1, 				pp->spriteys / 2);

	XFillRectangle(display, pp->ghostPixmap[0][0][0], bg_gc,
		0, 0, pp->spritexs, pp->spriteys);
	XFillArc(display, pp->ghostPixmap[0][0][0], fg_gc,
		0, 0, pp->spritexs, pp->spriteys, 0, 11520);
	XFillPolygon(display, pp->ghostPixmap[0][0][0], fg_gc,
		points, 9, Nonconvex, CoordModeOrigin);
	XFreeGC(display, bg_gc);
	XFreeGC(display, fg_gc);


	if (pp->pacmanPixmap[0][0] != None)
		for (dir = 0; dir < 4; dir++)
			for (mouth = 0; mouth < MAXMOUTH; mouth++)
				XFreePixmap(display, 
						  pp->pacmanPixmap[dir]
						  	[mouth]);

	for (dir = 0; dir < 4; dir++)
		for (mouth = 0; mouth < MAXMOUTH; mouth++) {
			if ((pp->pacmanPixmap[dir][mouth] = XCreatePixmap(
					display, MI_WINDOW(mi), 
					pp->spritexs, pp->spriteys, 1)) == 
					None) {
				free_pacman(display, pp);
				return;
			}
			gcv.foreground = 1;
        		gcv.background = 0;
			if ((fg_gc = XCreateGC(display, pp->pacmanPixmap[dir][mouth],
					GCForeground | GCBackground, &gcv)) == None) {
				free_pacman(display, pp);
				return;
			}
			gcv.foreground = 0;
        		gcv.background = 0;
			if ((bg_gc = XCreateGC(display, 
						pp->pacmanPixmap[dir][mouth],
						GCForeground | 
						GCBackground, &gcv)) == 
					None) {
        			XFreeGC(display, fg_gc);
				free_pacman(display, pp);
				return;
			}
			XFillRectangle(display, 
					pp->pacmanPixmap[dir][mouth], bg_gc,
					0, 0, pp->spritexs, pp->spriteys);
			if (pp->spritexs == 1 && pp->spriteys == 1)
				XFillRectangle(display, 
					     pp->pacmanPixmap[dir][mouth], 
					     fg_gc, 0, 0, pp->spritexs, 
					     pp->spriteys);
			else
				XFillArc(display, 
					       pp->pacmanPixmap[dir][mouth],
					       fg_gc,
					0, 0, pp->spritexs, pp->spriteys,
					((90 - dir * 90) + mouth * 5) * 64,
					(360 + (-2 * mouth * 5)) * 64);
			XFreeGC(display, fg_gc);
			XFreeGC(display, bg_gc);
		}
#endif /* USE_PIXMAP */

	pp->pacman.lastbox = START;
	pp->pacman.mouthdirection = 1;
	pp->pacman.nextcol = NOWHERE;
	pp->pacman.nextrow = NOWHERE;

	if (pp->ghosts != NULL) {
			free(pp->ghosts);
			pp->ghosts = (ghoststruct *) NULL;
	}
	pp->nghosts = GHOSTS;

	if (!pp->ghosts)
		if ((pp->ghosts = (ghoststruct *) calloc((size_t)pp->nghosts,
				sizeof (ghoststruct))) == NULL) {
			free_pacman(display, pp);
			return;
		}

	pp->pacman.mouthstage = MAXMOUTH - 1;

	MI_CLEARWINDOW(mi);
	repopulate(mi);
}

/* Callback function called for each tick.  This is the complete machinery of
   everything that moves. */
void
draw_pacman(ModeInfo * mi)
{
	unsigned int g;
	pacmangamestruct *pp;

	if (pacmangames == NULL)
		return;
	pp = &pacmangames[MI_SCREEN(mi)];
	if (pp->ghosts == NULL)
		return;

	pp->pacman.err.x = (pp->pacman.err.x + 1) % pp->pacman.speed;
	pp->pacman.err.y = (pp->pacman.err.y + 1) % pp->pacman.speed;
	pp->pacman.delta.x += pp->pacman.err.x != 0 ? pp->incx : 0;
	pp->pacman.delta.y += pp->pacman.err.y != 0 ? pp->incy : 0;

	if (pp->pacman.delta.x >= pp->xs && pp->pacman.delta.y >= pp->ys) {
		pac_update(mi, pp, &(pp->pacman));
		check_death(mi, pp);
		pp->pacman.delta.x = pp->incx;
		pp->pacman.delta.y = pp->incy;
	}

	if (pp->pacman.delta.x > pp->xs + pp->incx)
		pp->pacman.delta.x = pp->xs + pp->incx;
	if (pp->pacman.delta.y > pp->ys + pp->incy)
		pp->pacman.delta.y = pp->ys + pp->incy;

	for (g = 0; g < pp->nghosts; g++) {
		if (pp->ghosts[g].dead == True) continue;

		pp->ghosts[g].err.x = (pp->ghosts[g].err.x + 1) % 
			pp->ghosts[g].speed;
		pp->ghosts[g].err.y = (pp->ghosts[g].err.y + 1) % 
			pp->ghosts[g].speed;
		pp->ghosts[g].delta.x += pp->ghosts[g].err.x != 0 ? 
			pp->incx : 0;
		pp->ghosts[g].delta.y += pp->ghosts[g].err.y != 0 ? 
			pp->incy : 0;

		if (pp->ghosts[g].delta.x >= pp->xs && 
		    pp->ghosts[g].delta.y >= pp->ys) {
			ghost_update(pp, &(pp->ghosts[g]));
			pp->ghosts[g].delta.x = pp->incx;
			pp->ghosts[g].delta.y = pp->incy;
		}

		if (pp->ghosts[g].delta.x > pp->xs + pp->incx)
			pp->ghosts[g].delta.x = pp->xs + pp->incx;
		if (pp->ghosts[g].delta.y > pp->ys + pp->incy)
			pp->ghosts[g].delta.y = pp->ys + pp->incy;
	}

	pacman_tick(mi);
}

/* Releases resources. */
void
release_pacman(ModeInfo * mi)
{
	if (pacmangames != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
			free_pacman(MI_DISPLAY(mi), &pacmangames[screen]);
		free(pacmangames);
		pacmangames = (pacmangamestruct *) NULL;
	}
}

/* Refresh current level. */
void
refresh_pacman(ModeInfo * mi)
{
	drawlevel(mi);
	pacman_tick(mi);
}

/* Callback to change level. */
void
change_pacman(ModeInfo * mi)
{
	MI_CLEARWINDOW(mi);
	repopulate(mi);
}

#endif /* MODE_pacman */
