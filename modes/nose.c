/* -*- Mode: C; tab-width: 4 -*- */
/*-
 * nose --- a little guy with a big nose and a hat wanders around
 *          spewing out messages
 */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)nose.c	4.07 97/11/24 xlockmore";

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
 * Revision History:
 * 06-Jun-97: Compatible with xscreensaver and now colorized (idea from
 *            xscreensaver but colors are random).
 * 27-Feb-96: Added new ModeInfo arg to init and callback hooks.  Removed
 *		references to onepause, now uses MI_PAUSE(mi) interface.
 *		Ron Hitchens <ron@idiom.com>
 * 10-Oct-95: A better way of handling fortunes from a file, thanks to
 *            Jouk Jansen <joukj@crys.chem.uva.nl>.
 * 21-Sep-95: font option added, debugged for multiscreens
 * 12-Aug-95: xlock version
 * 1992: xscreensaver version, noseguy (Jamie Zawinski <jwz@netscape.com>)
 * 1990: X11 version, xnlock (Dan Heller <argv@sun.com>)
 */

/*-
 * xscreensaver, Copyright (c) 1992 Jamie Zawinski <jwz@mcom.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifdef STANDALONE
#define PROGCLASS "Nose"
#define HACK_INIT init_nose
#define HACK_DRAW draw_nose
#define nose_opts xlockmore_opts
#define DEFAULTS "*delay: 100000 \n" \
 "*ncolors: 64 \n"
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#endif /* STANDALONE */

ModeSpecOpt nose_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct   nose_description =
{"nose", "init_nose", "draw_nose", "release_nose",
 "refresh_nose", "init_nose", NULL, &nose_opts,
 100000, 1, 1, 1, 64, 1.0, "",
 "Shows a man with a big nose runs around spewing out messages", 0, NULL};

#endif

#include "bitmaps/nose-hat.xbm"
#include "bitmaps/nose-hatd.xbm"
#include "bitmaps/nose-facef.xbm"
#include "bitmaps/nose-faced.xbm"
#include "bitmaps/nose-facel.xbm"
#include "bitmaps/nose-facer.xbm"
#include "bitmaps/nose-shoef.xbm"
#include "bitmaps/nose-shoel.xbm"
#include "bitmaps/nose-shoer.xbm"
#include "bitmaps/nose-stepl.xbm"
#include "bitmaps/nose-stepr.xbm"

#define font_height(f) ((f == None) ? 8 : f->ascent + f->descent)

#define L 0
#define R 1
#define LSTEP 2
#define RSTEP 3
#define LF 4
#define RF 5
#define F 6
#define D 7
#define PIXMAPS 8
#define PIXMAP_SIZE 64
#define MOVE 0
#define TALK 1
#define FREEZE 2

extern XFontStruct *getFont(Display * display);
extern char *getWords(int screen, int screens);
extern int  isRibbon(void);

typedef struct {
	int         x, y, width, height;
} window_rect;

typedef struct {
	int         xs, ys;
	int         width, height;
	GC          text_fg_gc, text_bg_gc, noseGC[PIXMAPS];
	char       *words;
	int         x, y;
	int         tinymode;	/* walking or getting passwd */
	int         length, dir, lastdir;
	int         up;
	int         busyLoop;
	int         frames;
	int         state;
	Bool        talking;
	window_rect s;
	Pixmap      position[PIXMAPS];
} nosestruct;

static nosestruct *noses = NULL;

static XFontStruct *mode_font = None;

#define LEFT 001
#define RIGHT 002
#define DOWN 004
#define UP 010
#define FRONT 020
#define X_INCR 3
#define Y_INCR 2
#define YELLOW (MI_NPIXELS(mi) / 6)

#define COPY(d,g,c,p,np,x,y,w,h) XSetForeground(d,g,c);\
XSetStipple(d,g,p); XSetTSOrigin(d,g,x,y);\
XFillRectangle(d,np,g,x,y,w,h)

static void
pickClothes(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	nosestruct *np = &noses[MI_SCREEN(mi)];
	XGCValues   gcv;
	Pixmap      face_pix, hat_pix, shoe_pix, shoel_pix, shoer_pix;
	unsigned long hat_color = (MI_NPIXELS(mi) <= 2) ?
	MI_WHITE_PIXEL(mi) : MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
	unsigned long face_color = (MI_NPIXELS(mi) <= 2) ?
	MI_WHITE_PIXEL(mi) : MI_PIXEL(mi, (YELLOW));	/* Racism? */
	unsigned long shoe_color = (MI_NPIXELS(mi) <= 2) ?
	MI_WHITE_PIXEL(mi) : MI_PIXEL(mi, NRAND(MI_NPIXELS(mi)));
	int         i;

	gcv.graphics_exposures = False;
	gcv.foreground = MI_BLACK_PIXEL(mi);
	gcv.background = MI_BLACK_PIXEL(mi);
	for (i = 0; i < PIXMAPS; i++) {
		np->position[i] = XCreatePixmap(display, window, PIXMAP_SIZE, PIXMAP_SIZE,
						MI_DEPTH(mi));
		np->noseGC[i] = XCreateGC(display, np->position[i],
		    GCForeground | GCBackground | GCGraphicsExposures, &gcv);
		XFillRectangle(display, np->position[i], np->noseGC[i],
			       0, 0, PIXMAP_SIZE, PIXMAP_SIZE);
	}
	XSetBackground(display, gc, MI_BLACK_PIXEL(mi));
	XSetFillStyle(display, gc, FillStippled);
	/* DOWN NOSE GUY */
	hat_pix = XCreateBitmapFromData(display, window,
					(char *) nose_hat_down_bits,
				  nose_hat_down_width, nose_hat_down_height);
	face_pix = XCreateBitmapFromData(display, window,
					 (char *) nose_face_down_bits,
				nose_face_down_width, nose_face_down_height);
	shoe_pix = XCreateBitmapFromData(display, window,
					 (char *) nose_shoe_front_bits,
			      nose_shoe_front_width, nose_shoe_front_height);
	COPY(display, gc, shoe_color, shoe_pix, np->position[D],
	     (PIXMAP_SIZE - nose_shoe_front_width) / 2,
	     nose_hat_height + nose_face_front_height + 3,
	     nose_shoe_front_width, nose_shoe_front_height);
	COPY(display, gc, face_color, face_pix, np->position[D],
	  (PIXMAP_SIZE - nose_face_down_width) / 2, nose_hat_down_height + 7,
	     nose_face_down_width, nose_face_down_height);
	COPY(display, gc, hat_color, hat_pix, np->position[D],
	     (PIXMAP_SIZE - nose_hat_down_width) / 2, 7,
	     nose_hat_down_width, nose_hat_down_height);
	if (MI_NPIXELS(mi) <= 2) {
		XSetFillStyle(display, gc, FillSolid);
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, np->position[D], gc,
			       0, nose_hat_down_height + 6, PIXMAP_SIZE, 1);
		XSetFillStyle(display, gc, FillStippled);
	}
	XFreePixmap(display, face_pix);
	XFreePixmap(display, hat_pix);
	/* FRONT NOSE GUY */
	hat_pix = XCreateBitmapFromData(display, window,
					(char *) nose_hat_bits,
					nose_hat_width, nose_hat_height);
	face_pix = XCreateBitmapFromData(display, window,
					 (char *) nose_face_front_bits,
			      nose_face_front_width, nose_face_front_height);
	COPY(display, gc, shoe_color, shoe_pix, np->position[F],
	     (PIXMAP_SIZE - nose_shoe_front_width) / 2,
	     nose_hat_height + nose_face_front_height + 3,
	     nose_shoe_front_width, nose_shoe_front_height);
	COPY(display, gc, hat_color, hat_pix, np->position[F],
	     (PIXMAP_SIZE - nose_hat_width) / 2, 4,
	     nose_hat_width, nose_hat_height);
	if (MI_NPIXELS(mi) <= 2) {
		XSetFillStyle(display, gc, FillSolid);
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, np->position[F], gc,
			       0, nose_hat_height + 3, PIXMAP_SIZE, 1);
		XSetFillStyle(display, gc, FillStippled);
	}
	COPY(display, gc, face_color, face_pix, np->position[F],
	     (PIXMAP_SIZE - nose_face_front_width) / 2, nose_hat_height + 1,
	     nose_face_front_width, nose_face_front_height);
	XFreePixmap(display, shoe_pix);
	/* FRONT LEFT NOSE GUY */
	shoel_pix = XCreateBitmapFromData(display, window,
					  (char *) nose_shoe_left_bits,
				nose_shoe_left_width, nose_shoe_left_height);
	COPY(display, gc, shoe_color, shoel_pix, np->position[LF],
	     (PIXMAP_SIZE - nose_shoe_left_width) / 2 - 4,
	     nose_hat_height + nose_face_front_height + 3,
	     nose_shoe_left_width, nose_shoe_left_height);
	COPY(display, gc, hat_color, hat_pix, np->position[LF],
	     (PIXMAP_SIZE - nose_hat_width) / 2, 4,
	     nose_hat_width, nose_hat_height);
	if (MI_NPIXELS(mi) <= 2) {
		XSetFillStyle(display, gc, FillSolid);
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, np->position[LF], gc,
			       0, nose_hat_height + 3, PIXMAP_SIZE, 1);
		XSetFillStyle(display, gc, FillStippled);
	}
	COPY(display, gc, face_color, face_pix, np->position[LF],
	     (PIXMAP_SIZE - nose_face_front_width) / 2, nose_hat_height + 1,
	     nose_face_front_width, nose_face_front_height);
	/* FRONT RIGHT NOSE GUY */
	shoer_pix = XCreateBitmapFromData(display, window,
					  (char *) nose_shoe_right_bits,
			      nose_shoe_right_width, nose_shoe_right_height);
	COPY(display, gc, shoe_color, shoer_pix, np->position[RF],
	     (PIXMAP_SIZE - nose_shoe_right_width) / 2 + 4,
	     nose_hat_height + nose_face_front_height + 3,
	     nose_shoe_right_width, nose_shoe_right_height);
	COPY(display, gc, hat_color, hat_pix, np->position[RF],
	     (PIXMAP_SIZE - nose_hat_width) / 2, 4,
	     nose_hat_width, nose_hat_height);
	if (MI_NPIXELS(mi) <= 2) {
		XSetFillStyle(display, gc, FillSolid);
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, np->position[RF], gc,
			       0, nose_hat_height + 3, PIXMAP_SIZE, 1);
		XSetFillStyle(display, gc, FillStippled);
	}
	COPY(display, gc, face_color, face_pix, np->position[RF],
	     (PIXMAP_SIZE - nose_face_front_width) / 2, nose_hat_height + 1,
	     nose_face_front_width, nose_face_front_height);
	XFreePixmap(display, face_pix);
	/* LEFT NOSE GUY */
	face_pix = XCreateBitmapFromData(display, window,
					 (char *) nose_face_left_bits,
				nose_face_left_width, nose_face_left_height);
	COPY(display, gc, shoe_color, shoel_pix, np->position[L],
	     (PIXMAP_SIZE - nose_shoe_left_width) / 2 - 4,
	     nose_hat_height + nose_face_front_height + 3,
	     nose_shoe_left_width, nose_shoe_left_height);
	COPY(display, gc, hat_color, hat_pix, np->position[L],
	     (PIXMAP_SIZE - nose_hat_width) / 2, 4,
	     nose_hat_width, nose_hat_height);
	if (MI_NPIXELS(mi) <= 2) {
		XSetFillStyle(display, gc, FillSolid);
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, np->position[L], gc,
			       0, nose_hat_height + 3, PIXMAP_SIZE, 1);
		XSetFillStyle(display, gc, FillStippled);
	}
	COPY(display, gc, face_color, face_pix, np->position[L],
	   (PIXMAP_SIZE - nose_face_left_width) / 2 - 4, nose_hat_height + 4,
	     nose_face_left_width, nose_face_left_height);
	XFreePixmap(display, shoel_pix);
	/* LEFT NOSE GUY STEPPING */
	shoel_pix = XCreateBitmapFromData(display, window,
					  (char *) nose_step_left_bits,
				nose_step_left_width, nose_step_left_height);
	COPY(display, gc, shoe_color, shoel_pix, np->position[LSTEP],
	     (PIXMAP_SIZE - nose_step_left_width) / 2,
	     nose_hat_height + nose_face_front_height - 1,
	     nose_step_left_width, nose_step_left_height);
	COPY(display, gc, hat_color, hat_pix, np->position[LSTEP],
	     (PIXMAP_SIZE - nose_hat_width) / 2, 4,
	     nose_hat_width, nose_hat_height);
	if (MI_NPIXELS(mi) <= 2) {
		XSetFillStyle(display, gc, FillSolid);
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, np->position[LSTEP], gc,
			       0, nose_hat_height + 3, PIXMAP_SIZE, 1);
		XSetFillStyle(display, gc, FillStippled);
	}
	COPY(display, gc, face_color, face_pix, np->position[LSTEP],
	   (PIXMAP_SIZE - nose_face_left_width) / 2 - 4, nose_hat_height + 4,
	     nose_face_left_width, nose_face_left_height);
	XFreePixmap(display, face_pix);
	XFreePixmap(display, shoel_pix);
	/* RIGHT NOSE GUY */
	face_pix = XCreateBitmapFromData(display, window,
					 (char *) nose_face_right_bits,
			      nose_face_right_width, nose_face_right_height);
	COPY(display, gc, shoe_color, shoer_pix, np->position[R],
	     (PIXMAP_SIZE - nose_shoe_right_width) / 2 + 4,
	     nose_hat_height + nose_face_front_height + 3,
	     nose_shoe_right_width, nose_shoe_right_height);
	COPY(display, gc, hat_color, hat_pix, np->position[R],
	     (PIXMAP_SIZE - nose_hat_width) / 2, 4,
	     nose_hat_width, nose_hat_height);
	if (MI_NPIXELS(mi) <= 2) {
		XSetFillStyle(display, gc, FillSolid);
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, np->position[R], gc,
			       0, nose_hat_height + 3, PIXMAP_SIZE, 1);
		XSetFillStyle(display, gc, FillStippled);
	}
	COPY(display, gc, face_color, face_pix, np->position[R],
	  (PIXMAP_SIZE - nose_face_right_width) / 2 + 4, nose_hat_height + 4,
	     nose_face_right_width, nose_face_right_height);
	XFreePixmap(display, shoer_pix);
	/* RIGHT NOSE GUY STEPPING */
	shoer_pix = XCreateBitmapFromData(display, window,
					  (char *) nose_step_right_bits,
			      nose_step_right_width, nose_step_right_height);
	COPY(display, gc, shoe_color, shoer_pix, np->position[RSTEP],
	     (PIXMAP_SIZE - nose_step_right_width) / 2,
	     nose_hat_height + nose_face_front_height - 1,
	     nose_step_right_width, nose_step_right_height);
	COPY(display, gc, hat_color, hat_pix, np->position[RSTEP],
	     (PIXMAP_SIZE - nose_hat_width) / 2, 4,
	     nose_hat_width, nose_hat_height);
	if (MI_NPIXELS(mi) <= 2) {
		XSetFillStyle(display, gc, FillSolid);
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
		XFillRectangle(display, np->position[RSTEP], gc,
			       0, nose_hat_height + 3, PIXMAP_SIZE, 1);
		XSetFillStyle(display, gc, FillStippled);
	}
	COPY(display, gc, face_color, face_pix, np->position[RSTEP],
	  (PIXMAP_SIZE - nose_face_right_width) / 2 + 4, nose_hat_height + 4,
	     nose_face_right_width, nose_face_right_height);
	XFreePixmap(display, face_pix);
	XFreePixmap(display, shoer_pix);
	XFreePixmap(display, hat_pix);
	XSetFillStyle(display, gc, FillSolid);
}

static void
walk(ModeInfo * mi, register int dir)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	nosestruct *np = &noses[MI_SCREEN(mi)];
	register int incr = 0;

	if (dir & (LEFT | RIGHT)) {	/* left/right movement (mabye up/down too) */
		np->up = -np->up;	/* bouncing effect (even if hit a wall) */
		if (dir & LEFT) {
			incr = X_INCR;
			np->frames = (np->up < 0) ? L : LSTEP;
		} else {
			incr = -X_INCR;
			np->frames = (np->up < 0) ? R : RSTEP;
		}
		/* note that maybe neither UP nor DOWN is set! */
		if (dir & UP && np->y > Y_INCR)
			np->y -= Y_INCR;
		else if (dir & DOWN && np->y < np->height - np->ys)
			np->y += Y_INCR;
	} else if (dir == UP) {	/* Explicit up/down movement only (no left/right) */
		np->y -= Y_INCR;
		XCopyArea(display, np->position[F], window, np->noseGC[F],
			  0, 0, PIXMAP_SIZE, PIXMAP_SIZE, np->x, np->y);
	} else if (dir == DOWN) {
		np->y += Y_INCR;
		XCopyArea(display, np->position[D], window, np->noseGC[D],
			  0, 0, PIXMAP_SIZE, PIXMAP_SIZE, np->x, np->y);
	} else if (dir == FRONT && np->frames != F) {
		if (np->up > 0)
			np->up = -np->up;
		if (np->lastdir & LEFT)
			np->frames = LF;
		else if (np->lastdir & RIGHT)
			np->frames = RF;
		else
			np->frames = F;
		XCopyArea(display, np->position[np->frames], window, np->noseGC[np->frames],
			  0, 0, PIXMAP_SIZE, PIXMAP_SIZE, np->x, np->y);
	}
	if (dir & LEFT)
		while (--incr >= 0) {
			--np->x;
			XCopyArea(display, np->position[np->frames], window, np->noseGC[np->frames],
				  0, 0, PIXMAP_SIZE, PIXMAP_SIZE, np->x, np->y + np->up);
			XFlush(display);
	} else if (dir & RIGHT)
		while (++incr <= 0) {
			++np->x;
			XCopyArea(display, np->position[np->frames], window, np->noseGC[np->frames],
				  0, 0, PIXMAP_SIZE, PIXMAP_SIZE, np->x, np->y + np->up);
			XFlush(display);
		}
	np->lastdir = dir;
}

static int
think(ModeInfo * mi)
{
	nosestruct *np = &noses[MI_SCREEN(mi)];

	if (LRAND() & 1)
		walk(mi, FRONT);
	if (LRAND() & 1) {
		np->words = getWords(MI_SCREEN(mi), MI_NUM_SCREENS(mi));
		return 1;
	}
	return 0;
}

#define MAXLINES 40
#if 0
#define MAXWIDTH BUFSIZ
#else
#define MAXWIDTH 170
#endif

/*-
Strange but true:
On an HP Pa-RISC with gcc MAXLINES * MAXWIDTH should be <=
around 6800 (40 * 170) or

as: /usr/tmp/cca####.s line#3364 [err#13]
  (warning) Use of GR3 when frame>=8192 may cause conflict

*/

static void
talk(ModeInfo * mi, Bool force_erase)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	nosestruct *np = &noses[MI_SCREEN(mi)];
	int         width = 0, height, Y, Z, total = 0;
	register char *p, *p2;
	char        buf[BUFSIZ], args[MAXLINES][MAXWIDTH];

	/* clear what we've written */
	if (np->talking || force_erase) {
		if (!np->talking)
			return;
		XFillRectangle(display, window, np->text_bg_gc, np->s.x - 5, np->s.y - 5,
			       np->s.width + 10, np->s.height + 10);
		np->talking = False;
		if (!force_erase)
			np->state = MOVE;
		return;
	}
	np->talking = True;
	walk(mi, FRONT);
	p = strncpy(buf, np->words, BUFSIZ);
	if (!(p2 = (char *) strchr(p, '\n')) || !p2[1]) {
		(void) strncpy(args[0], np->words, MAXWIDTH - 1);
		total = strlen(args[0]);
		args[0][MAXWIDTH - 1] = '\0';
		if (mode_font == None)
			width = 8;
		else
			width = XTextWidth(mode_font, np->words, total);
		height = 0;
	} else
		/* p2 now points to the first '\n' */
		for (height = 0; p && height < MAXLINES; height++) {
			int         w;

			*p2 = 0;
			if (mode_font != None && (w = XTextWidth(mode_font, p, p2 - p)) > width)
				width = w;
			total += p2 - p;	/* total chars; count to determine reading time */
			(void) strncpy(args[height], p, MAXWIDTH - 1);
			args[height][MAXWIDTH - 1] = '\0';
			p = p2 + 1;
			if (!(p2 = (char *) strchr(p, '\n')))
				break;
		}

	height++;
	/*
	 * Figure out the height and width in imagepixels (height, width) extend the
	 * new box by 15 pixels on the sides (30 total) top and bottom.
	 */
	np->s.width = width + 30;
	np->s.height = height * font_height(mode_font) + 30;
	if (np->x - np->s.width - 10 < 5)
		np->s.x = 5;
	else if ((np->s.x = np->x + 32 - (np->s.width + 15) / 2)
		 + np->s.width + 15 > np->width - 5)
		np->s.x = np->width - 15 - np->s.width;
	if (np->y - np->s.height - 10 < 5)
		np->s.y = np->y + np->ys + 5;
	else
		np->s.y = np->y - 5 - np->s.height;

	XFillRectangle(display, window, np->text_bg_gc,
		       np->s.x, np->s.y, np->s.width, np->s.height);

	/* make a box that's 5 pixels thick. Then add a thin box inside it */
	XSetLineAttributes(display, np->text_fg_gc, 5, 0, 0, 0);
	XDrawRectangle(display, window, np->text_fg_gc,
		       np->s.x, np->s.y, np->s.width - 1, np->s.height - 1);
	XSetLineAttributes(display, np->text_fg_gc, 0, 0, 0, 0);
	XDrawRectangle(display, window, np->text_fg_gc,
	      np->s.x + 7, np->s.y + 7, np->s.width - 15, np->s.height - 15);

	Y = 15 + font_height(mode_font);

	/* now print each string in reverse order (start at bottom of box) */
	for (Z = 0; Z < height; Z++) {
		(void) XDrawString(display, window, np->text_fg_gc,
			np->s.x + 15, np->s.y + Y, args[Z], strlen(args[Z]));
		Y += font_height(mode_font);
	}
	np->busyLoop = (total / 15) * 10;
	if (np->busyLoop < 30)
		np->busyLoop = 30;
	np->state = TALK;
}

static int
look(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);

	/*GC          gc = MI_GC(mi); */
	nosestruct *np = &noses[MI_SCREEN(mi)];
	int         i;

	if (NRAND(3)) {
		i = (LRAND() & 1) ? D : F;
		XCopyArea(display, np->position[i], window, np->noseGC[i],
			  0, 0, PIXMAP_SIZE, PIXMAP_SIZE, np->x, np->y);
		return 3;
	}
	if (!NRAND(5))
		return 0;
	if (NRAND(3)) {
		i = (LRAND() & 1) ? LF : RF;
		XCopyArea(display, np->position[i], window, np->noseGC[i],
			  0, 0, PIXMAP_SIZE, PIXMAP_SIZE, np->x, np->y);
		return 3;
	}
	if (!NRAND(5))
		return 0;
	i = (LRAND() & 1) ? L : R;
	XCopyArea(display, np->position[i], window, np->noseGC[i],
		  0, 0, PIXMAP_SIZE, PIXMAP_SIZE, np->x, np->y);
	return 3;
}

static void
move(ModeInfo * mi)
{
	nosestruct *np = &noses[MI_SCREEN(mi)];

	if (!np->length) {
		register int tries = 0;

		np->dir = 0;
		if ((LRAND() & 1) && think(mi)) {
			talk(mi, False);	/* sets timeout to itself */
			return;
		}
		if (!NRAND(3) && (np->busyLoop = look(mi))) {
			np->state = MOVE;
			return;
		}
		np->busyLoop = 3 + NRAND(3);
		do {
			if (!tries)
				np->length = np->width / 100 + NRAND(90), tries = 8;
			else
				tries--;
			switch (NRAND(8)) {
				case 0:
					if (np->x - X_INCR * np->length >= 5)
						np->dir = LEFT;
					break;
				case 1:
					if (np->x + X_INCR * np->length <= np->width - np->xs - 6)
						np->dir = RIGHT;
					break;
				case 2:
					if (np->y - (Y_INCR * np->length) >= 5)
						np->dir = UP;
					break;
				case 3:
					if (np->y + Y_INCR * np->length <= np->height - np->ys - 6)
						np->dir = DOWN;
					break;
				case 4:
					if (np->x - X_INCR * np->length >= 5 &&
					  np->y - (Y_INCR * np->length) >= 5)
						np->dir = (LEFT | UP);
					break;
				case 5:
					if (np->x + X_INCR * np->length <= np->width - np->xs - 6 &&
					    np->y - Y_INCR * np->length >= 5)
						np->dir = (RIGHT | UP);
					break;
				case 6:
					if (np->x - X_INCR * np->length >= 5 &&
					    np->y + Y_INCR * np->length <= np->height - np->ys - 6)
						np->dir = (LEFT | DOWN);
					break;
				case 7:
					if (np->x + X_INCR * np->length <= np->width - np->xs - 6 &&
					    np->y + Y_INCR * np->length <= np->height - np->ys - 6)
						np->dir = (RIGHT | DOWN);
					break;
				default:
					/* No Defaults */
					break;
			}
		} while (!np->dir);
	}
	walk(mi, np->dir);
	--np->length;
	np->state = MOVE;
}

void
init_nose(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc = MI_GC(mi);
	nosestruct *np;
	XGCValues   gcv;
	int         i = 0;

	if (noses == NULL) {
		if ((noses = (nosestruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (nosestruct))) == NULL)
			return;
	}
	np = &noses[MI_SCREEN(mi)];

	np->width = MI_WIN_WIDTH(mi) + 2;
	np->height = MI_WIN_HEIGHT(mi) + 2;
	np->tinymode = (np->width + np->height < 4 * PIXMAP_SIZE);
	np->xs = PIXMAP_SIZE;
	np->ys = PIXMAP_SIZE;

	MI_CLEARWINDOW(mi);

	XSetForeground(display, gc, MI_WHITE_PIXEL(mi));

	/* don't want any exposure events from XCopyPlane */
	XSetGraphicsExposures(display, gc, False);

	if (mode_font == None)
		mode_font = getFont(display);
	if (np->noseGC[0] == NULL)
		pickClothes(mi);
	np->words = getWords(MI_SCREEN(mi), MI_NUM_SCREENS(mi));
	if (np->text_fg_gc == NULL && mode_font != None) {
		gcv.font = mode_font->fid;
		XSetFont(display, gc, mode_font->fid);
		gcv.graphics_exposures = False;
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		np->text_fg_gc = XCreateGC(display, window,
					   GCForeground | GCBackground | GCGraphicsExposures | GCFont, &gcv);
		gcv.foreground = MI_BLACK_PIXEL(mi);
		gcv.background = MI_WHITE_PIXEL(mi);
		np->text_bg_gc = XCreateGC(display, window,
					   GCForeground | GCBackground | GCGraphicsExposures | GCFont, &gcv);
	}
	np->up = 1;
	if (np->tinymode) {
		np->x = 0;
		np->y = 0;
		i = (NRAND(PIXMAPS));
		XCopyArea(display, np->position[i], window, np->noseGC[i],
			  0, 0, PIXMAP_SIZE, PIXMAP_SIZE,
			  (np->width - PIXMAP_SIZE) / 2,
			  (np->height - PIXMAP_SIZE) / 2);
		np->state = FREEZE;
	} else {
		np->x = np->width / 2;
		np->y = np->height / 2;
		np->state = MOVE;
	}
	XFlush(display);
}

void
draw_nose(ModeInfo * mi)
{
	nosestruct *np = &noses[MI_SCREEN(mi)];

	if (np->busyLoop > 0) {
		np->busyLoop--;
		return;
	}
	switch (np->state) {
		case MOVE:
			move(mi);
			break;
		case TALK:
			talk(mi, 0);
			break;
	}
}

void
release_nose(ModeInfo * mi)
{
	if (noses != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			Display    *display = MI_DISPLAY(mi);
			nosestruct *np = &noses[screen];
			int         i;

			if (np->text_fg_gc != NULL)
				XFreeGC(display, np->text_fg_gc);
			if (np->text_bg_gc != NULL)
				XFreeGC(display, np->text_bg_gc);
			for (i = 0; i < PIXMAPS; i++) {
				if (np->position[i])
					XFreePixmap(display, np->position[i]);
				if (np->noseGC[i] != NULL)
					XFreeGC(display, np->noseGC[i]);
			}
		}
		(void) free((void *) noses);
		noses = NULL;
	}
	if (mode_font != None) {
		XFreeFont(MI_DISPLAY(mi), mode_font);
		mode_font = None;
	}
}

void
refresh_nose(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}
