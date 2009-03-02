/* -*- Mode: C; tab-width: 4 -*- */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)flag.c	4.03 97/05/10 xlockmore";

#endif

/*-
 * flag.c - flag for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1996 by Charles Vidal <vidalc@univ-mlv.fr>
 *
 * Thanks to Bas van Gaalen, Holland, PD, for his Pascal source
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
 * 13-May-97: jwz@netscape.com: turned into a standalone program.
 *            Made it able to animate arbitrary (runtime) text or bitmaps
 * 15-May-96: written.
 */

#ifdef STANDALONE
#define PROGCLASS            "Flag"
#define HACK_INIT            init_flag
#define HACK_DRAW            draw_flag
#define DEF_DELAY            50000
#define DEF_CYCLES           1000
#define DEF_SIZE             -7
#define DEF_NCOLORS          200
#define BRIGHT_COLORS
#define UNIFORM_COLORS
#include <X11/Xutil.h>
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

#define DEF_INVERT  "False"
 
static Bool invert;
 
#ifndef STANDALONE
static XrmOptionDescRec opts[] =
{
  {"-invert", ".flag.invert", XrmoptionNoArg, (caddr_t) "on"},
  {"+invert", ".flag.invert", XrmoptionNoArg, (caddr_t) "off"}
};
static argtype vars[] =
{
  {(caddr_t *) & invert, "invert", "Invert", DEF_INVERT, t_Bool}
};
static OptionStruct desc[] =
{
  {"-/+invert", "turn on/off inverting of flag"}
};
 
ModeSpecOpt flag_opts =
{2, opts, 1, vars, desc};
#endif /* !STANDALONE */



#if !defined( VMS ) || ( __VMS_VER >= 70000000 )
#include <sys/utsname.h>
#else
#if USE_XVMSUTILS
#if 0
#include "../xvmsutils/utsname.h"
#else
#include <X11/utsname.h>
#endif
#endif /* USE_XVMSUTILS */
#endif
#include "flag.xbm"

/* aliases for vars defined in the bitmap file */
#define FLAG_WIDTH   image_width
#define FLAG_HEIGHT    image_height
#define FLAG_BITS    image_bits

#define MINSIZE 1
#define MAXSCALE 8
#define MINSCALE 2
#define MAXINITSIZE 6
#define MININITSIZE 2
#define MINAMP 5
#define MAXAMP 20
#define MAXW(fp) (MAXSCALE * (fp)->image->width + 2 * MAXAMP + (fp)->pointsize)
#define MAXH(fp) (MAXSCALE * (fp)->image->height+ 2 * MAXAMP + (fp)->pointsize)
#define MINW(fp) (MINSCALE * (fp)->image->width + 2 * MINAMP + (fp)->pointsize)
#define MINH(fp) (MINSCALE * (fp)->image->height+ 2 * MINAMP + (fp)->pointsize)
#define ANGLES		360

typedef struct {
	int         samp;
	int         sofs;
	int         sidx;
	int         x_flag, y_flag;
	int         timer;
	int         initialized;
	int         stab[ANGLES];
	Pixmap      cache;
	int         width, height;
	int         pointsize;
	float       size;
	float       inctaille;
	int         startcolor;
	int         choice;
	XImage     *image;
} flagstruct;

static XImage *logo = NULL;
static int  graphics_format = -1;

static XFontStruct *mode_font = None;

static flagstruct *flags = NULL;

extern XFontStruct *getFont(Display * display);

static int
random_num(int n)
{
	return ((int) (((float) LRAND() / MAXRAND) * (n + 1.0)));
}

static void
initSintab(ModeInfo * mi)
{
	flagstruct *fp = &flags[MI_SCREEN(mi)];
	int         i;

	for (i = 0; i < ANGLES; i++)
		fp->stab[i] = (int) (SINF(i * 4 * M_PI / ANGLES) * fp->samp) + fp->sofs;
}

static void
affiche(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	int         x, y, xp, yp;
	flagstruct *fp = &flags[MI_SCREEN(mi)];

	for (x = 0; x < fp->image->width; x++)
		for (y = fp->image->height - 1; y >= 0; y--) {
			xp = (int) (fp->size * (float) x) +
				fp->stab[(fp->sidx + x + y) % ANGLES];
			yp = (int) (fp->size * (float) y) +
				fp->stab[(fp->sidx + 4 * x + y + y) % ANGLES];
			if (((int) !invert) ^ XGetPixel(fp->image, x, y))
				XSetForeground(display, MI_GC(mi), MI_WIN_BLACK_PIXEL(mi));
			else if (MI_NPIXELS(mi) <= 2)
				XSetForeground(display, MI_GC(mi), MI_WIN_WHITE_PIXEL(mi));
			else
				XSetForeground(display, MI_GC(mi),
					       MI_PIXEL(mi, (y + x + fp->sidx + fp->startcolor) %
							MI_NPIXELS(mi)));
			if (fp->pointsize <= 1)
				XDrawPoint(display, fp->cache, MI_GC(mi), xp, yp);
			else
#ifndef NOTHREED_EFFECT
				XFillRectangle(display, fp->cache, MI_GC(mi), xp, yp,
					       fp->pointsize, fp->pointsize);
#else
			if (fp->pointsize < 6)
				XFillRectangle(display, fp->cache, MI_GC(mi), xp, yp,
					       fp->pointsize, fp->pointsize);
			else
				XFillArc(display, fp->cache, MI_GC(mi), xp, yp,
				  fp->pointsize, fp->pointsize, 0, 360 * 64);
#endif
		}
}

extern char *message, *imagefile;

static void
getText(ModeInfo * mi, XImage ** image)
{
	Display    *display = MI_DISPLAY(mi);
	char       *text1, *text2;
	char       *line, *token;
	int         width, height;
	int         lines;
	int         margin = 2;
	XCharStruct overall;
	XGCValues   gcv;
	GC          gc;
	Pixmap      text_pixmap;


	if (!message || !*message) {
#if !defined( VMS ) || ( __VMS_VER >= 70000000 ) || defined( USE_XVMSUTILS )
		struct utsname uts;

		if (uname(&uts) < 0) {
			text1 = strdup("uname() failed");
		} else {
			char       *s;

			if ((s = strchr(uts.nodename, '.')))
				*s = 0;
			text1 = (char *) malloc(strlen(uts.nodename) +
						strlen(uts.sysname) +
						strlen(uts.release) + 10);
			(void) sprintf(text1, "%s\n%s %s",
				     uts.nodename, uts.sysname, uts.release);
		}
#else
		text1 = strdup("OpenVMS"); /* It says release 0 in my utsname.h */
#endif		
	} else {
		text1 = strdup(message);
	}
	while (*text1 && (text1[strlen(text1) - 1] == '\r' ||
			  text1[strlen(text1) - 1] == '\n'))
		text1[strlen(text1) - 1] = 0;
	text2 = strdup(text1);


	if (mode_font == None)
		mode_font = getFont(display);

	(void) memset(&overall, 0, sizeof (overall));
	token = text1;
	lines = 0;
	while ((line = strtok(token, "\r\n"))) {
		XCharStruct o2;
		int         ascent, descent, direction;

		token = 0;
		XTextExtents(mode_font, line, strlen(line),
			     &direction, &ascent, &descent, &o2);
		overall.lbearing = MAX(overall.lbearing, o2.lbearing);
		overall.rbearing = MAX(overall.rbearing, o2.rbearing);
		lines++;
	}

	width = overall.lbearing + overall.rbearing + margin + margin + 1;
	height = ((mode_font->ascent + mode_font->descent) * lines) +
		margin + margin;

	text_pixmap = XCreatePixmap(display, MI_WINDOW(mi), width, height, 1);

	gcv.font = mode_font->fid;
	gcv.foreground = 0;
	gcv.background = 0;
	gc = XCreateGC(display, text_pixmap,
		GCFont | GCForeground | GCBackground, &gcv);
	XFillRectangle(display, text_pixmap, gc, 0, 0, width, height);
	XSetForeground(display, gc, 1);

	token = text2;
	lines = 0;
	while ((line = strtok(token, "\r\n"))) {
		XCharStruct o2;
		int         ascent, descent, direction, xoff;

		token = 0;

		XTextExtents(mode_font, line, strlen(line),
			     &direction, &ascent, &descent, &o2);
		xoff = ((overall.lbearing + overall.rbearing) -
			(o2.lbearing + o2.rbearing)) / 2;

		XDrawString(display, text_pixmap, gc,
			    overall.lbearing + margin + xoff,
			    ((mode_font->ascent * (lines + 1)) +
			     (mode_font->descent * lines) + margin),
			    line, strlen(line));
		lines++;
	}
	(void) free((void *) text1);
	(void) free((void *) text2);
	/*XUnloadFont(display, mode_font->fid); */
	XFreeGC(display, gc);

	*image = XGetImage(display, text_pixmap, 0, 0, width, height,
			   1L, XYPixmap);
	XFreePixmap(MI_DISPLAY(mi), text_pixmap);
}

void
init_flag(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
  GC          gc = MI_GC(mi);
	int         size = MI_SIZE(mi);
	flagstruct *fp;

	if (flags == NULL) {
		if ((flags = (flagstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (flagstruct))) == NULL)
			return;
	}
	fp = &flags[MI_SCREEN(mi)];

	fp->width = MI_WIN_WIDTH(mi);
	fp->height = MI_WIN_HEIGHT(mi);

	if (!logo) {
		getBitmap(&logo, FLAG_WIDTH, FLAG_HEIGHT, FLAG_BITS,
			  &graphics_format);
	}
	if (fp->image == NULL) {
		if (MI_WIN_IS_FULLRANDOM(mi) ||
		    (imagefile && *imagefile && message && *message) ||
		    ((!imagefile || !*imagefile) && (!message || !*message)))
			fp->choice = LRAND() & 1;
		else if (imagefile && *imagefile)
			fp->choice = 0;
		else
			fp->choice = 1;
		if (fp->choice) {
			getText(mi, &fp->image);
		} else {
			fp->image = XCreateImage(display, MI_VISUAL(mi), 1, XYBitmap, 0,
				logo->data, logo->width, logo->height, 8, 0);
			fp->image->byte_order = LSBFirst;
			fp->image->bitmap_bit_order = LSBFirst;
		}
	}
	fp->samp = MAXAMP;	/* Amplitude */
	fp->sofs = 20;		/* ???????? */
	fp->pointsize = size;
	if (size < -MINSIZE)
		fp->pointsize = NRAND(-size - MINSIZE + 1) + MINSIZE;
	if (fp->pointsize < MINSIZE ||
	    fp->width <= MAXW(fp) || fp->height <= MAXH(fp))
		fp->pointsize = MINSIZE;
	fp->size = MAXINITSIZE;	/* Initial distance between pts */
	fp->inctaille = 0.05;
	fp->timer = 0;
	fp->sidx = fp->x_flag = fp->y_flag = 0;

	if (!fp->initialized) {
		fp->initialized = True;
		if (!(fp->cache = XCreatePixmap(display, MI_WINDOW(mi),
				      MAXW(fp), MAXH(fp), MI_WIN_DEPTH(mi))))
			return;
	}
	XSetForeground(display, gc, MI_WIN_BLACK_PIXEL(mi));
	XSetBackground(display, gc, MI_WIN_BLACK_PIXEL(mi));
	XFillRectangle(display, fp->cache, gc,
		       0, 0, MAXW(fp), MAXH(fp));
	/* don't want any exposure events from XCopyArea */
	XSetGraphicsExposures(display, gc, False);
	if (MI_NPIXELS(mi) > 2)
		fp->startcolor = NRAND(MI_NPIXELS(mi));
	if (fp->width <= MAXW(fp) || fp->height <= MAXH(fp)) {
		fp->samp = MINAMP;
		fp->sofs = 0;
		fp->x_flag = random_num(fp->width - MINW(fp));
		fp->y_flag = random_num(fp->height - MINH(fp));
	} else {
		fp->samp = MAXAMP;
		fp->sofs = 20;
		fp->x_flag = random_num(fp->width - MAXW(fp));
		fp->y_flag = random_num(fp->height - MAXH(fp));
	}

	initSintab(mi);

	XClearWindow(display, MI_WINDOW(mi));
}

void
draw_flag(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
  GC          gc = MI_GC(mi);
	flagstruct *fp = &flags[MI_SCREEN(mi)];

	if (fp->width <= MAXW(fp) || fp->height <= MAXH(fp)) {
		fp->size = MININITSIZE;
		/* fp->pointsize = MINPOINTSIZE; */
		XCopyArea(display, fp->cache, window, gc,
			  0, 0, MINW(fp), MINH(fp), fp->x_flag, fp->y_flag);
	} else {
		if ((fp->size + fp->inctaille) > MAXSCALE)
			fp->inctaille = -fp->inctaille;
		if ((fp->size + fp->inctaille) < MINSCALE)
			fp->inctaille = -fp->inctaille;
		fp->size += fp->inctaille;
		XCopyArea(display, fp->cache, window, gc,
			  0, 0, MAXW(fp), MAXH(fp), fp->x_flag, fp->y_flag);
	}
	XSetForeground(MI_DISPLAY(mi), gc, MI_WIN_BLACK_PIXEL(mi));
	XFillRectangle(display, fp->cache, gc,
		       0, 0, MAXW(fp), MAXH(fp));
	XFlush(display);
	affiche(mi);
	fp->sidx += 2;
	fp->sidx %= (ANGLES * MI_NPIXELS(mi));
	XFlush(display);
	fp->timer++;
	if ((MI_CYCLES(mi) > 0) && (fp->timer >= MI_CYCLES(mi)))
		init_flag(mi);
}

void
release_flag(ModeInfo * mi)
{
	if (flags != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			flagstruct *fp = &flags[screen];

			if (fp->cache)
				XFreePixmap(MI_DISPLAY(mi), fp->cache);
			if (fp->image) {
				if (fp->choice)
					XDestroyImage(fp->image);
				else
					XFree(fp->image); /* not XDestroyImage, still need the data */
			}
		}
		(void) free((void *) flags);
		flags = NULL;
	}
	if (mode_font != None) {
		XFreeFont(MI_DISPLAY(mi), mode_font);
		mode_font = None;
	}
	destroyBitmap(&logo, &graphics_format);
}

void
refresh_flag(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}
