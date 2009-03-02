/* -*- Mode: C; tab-width: 4 -*- */
/* flag --- a waving flag image */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)flag.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * Copyright (c) 1996 by Charles Vidal <vidalc@club-internet.fr>
 *         http://www.chez.com/vidalc
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
 * 24-Oct-97: xpm and ras capability added.
 * 13-May-97: jwz@jwz.org: turned into a standalone program.
 *            Made it able to animate arbitrary (runtime) text or bitmaps
 * 15-May-96: written.
 */

#ifdef STANDALONE
#define PROGCLASS "Flag"
#define HACK_INIT init_flag
#define HACK_DRAW draw_flag
#define flag_opts xlockmore_opts
#define DEFAULTS "*delay: 50000 \n" \
 "*cycles: 1000 \n" \
 "*size: -7 \n" \
 "*ncolors: 200 \n" \
 "*bitmap: \n" \
 "*font: \n" \
 "*text: \n" \
 "*fullrandom: True \n"
#define BRIGHT_COLORS
#define UNIFORM_COLORS
#define DEF_FONT "-*-helvetica-bold-r-*-240-*"
#define DEF_TEXT ""
#include "xlockmore.h"		/* in xscreensaver distribution */

#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */
#include "color.h"
#endif /* STANDALONE */

/* This makes me laugh :) */
#ifdef JWZ
/*-
 * 22-Jan-98: jwz: made the flag wigglier; added xpm support.
 *            (I tried to do this by re-porting from xlockmore, but the
 *            current xlockmore version is completely inscrutable.)
 */

#ifdef HAVE_XPM
#include <X11/xpm.h>
#ifndef PIXEL_ALREADY_TYPEDEFED
#define PIXEL_ALREADY_TYPEDEFED	/* Sigh, Xmu/Drawing.h needs this... */
#endif
#endif

#ifdef HAVE_XMU
#ifndef VMS
#include <X11/Xmu/Drawing.h>
#else /* VMS */
#include <Xmu/Drawing.h>
#endif /* VMS */
#endif /* HAVE_XMU */

#include "images/bob.xbm"
#include <string.h>
#include <X11/Xutil.h>
#else
#include "iostuff.h"
#endif

#ifdef MODE_flag

#define DEF_INVERT  "False"

static Bool invert;

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
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   flag_description =
{"flag", "init_flag", "draw_flag", "release_flag",
 "refresh_flag", "init_flag", NULL, &flag_opts,
 50000, 1, 1000, -7, 64, 1.0, "",
 "Shows a waving flag image", 0, NULL};

#endif

/* aliases for vars defined in the bitmap file */
#define FLAG_WIDTH   image_width
#define FLAG_HEIGHT    image_height
#define FLAG_BITS    image_bits

#include "flag.xbm"

#if defined( USE_XPM ) || defined( USE_XPMINC )
#define FLAG_NAME  image_name
#include "flag.xpm"
#define DEFAULT_XPM 0
#endif

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

#define MINSIZE 1
#define MAXSCALE 8
#define MINSCALE 2
#define MAXINITSIZE 6
#define MININITSIZE 2
#define MINAMP 5
#define MAXAMP 20
#define MAXW(fp) (MAXSCALE * (fp)->image->width + 2 * MAXAMP + (fp)->pointsize + 2 * (fp)->sofs)
#define MAXH(fp) (MAXSCALE * (fp)->image->height+ 2 * MAXAMP + (fp)->pointsize + 2 * (fp)->sofs)
#define MINW(fp) (MINSCALE * (fp)->image->width + 2 * MINAMP + (fp)->pointsize)
#define MINH(fp) (MINSCALE * (fp)->image->height+ 2 * MINAMP + (fp)->pointsize)
#define ANGLES		360
#define IMAGE_FLAG 0
#define MESSAGE_FLAG 1

typedef struct {
	int         samp;
	int         sofs;
	int         sidx;
	int         x_flag, y_flag;
	int         timer;
	int         stab[ANGLES];
	Pixmap      cache;
	int         width, height;
	int         pointsize;
	float       size;
	float       inctaille;
	int         startcolor;
	int         choice;
	XImage     *image;
	XImage     *logo;
	Colormap    cmap;
	unsigned long black;
	int         graphics_format;
	GC          backGC;
} flagstruct;

static XFontStruct *messagefont = None;

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

  /*-
   * change the periodicity of the sin formula : the maximum of the
   * periocity seem to be 16 ( 2^4 ), after the drawing isn't good looking
   */
	int         periodicity = random_num(4);
	int         puissance = 1;

	/* for (i=0;i<periodicity;i++) puissance*=2; */
	puissance <<= periodicity;
	for (i = 0; i < ANGLES; i++)
		fp->stab[i] = (int) (SINF(i * puissance * M_PI / ANGLES) * fp->samp) +
			fp->sofs;
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
			if (MI_NPIXELS(mi) <= 2 || fp->graphics_format < IS_XPM ||
			    fp->choice == MESSAGE_FLAG) {
				if (((int) !invert) ^ XGetPixel(fp->image, x, y))
					XSetForeground(display, fp->backGC, fp->black);
				else if (MI_NPIXELS(mi) <= 2)
					XSetForeground(display, fp->backGC, MI_WHITE_PIXEL(mi));
				else
					XSetForeground(display, fp->backGC,
						       MI_PIXEL(mi, (y + x + fp->sidx + fp->startcolor) %
							    MI_NPIXELS(mi)));
			} else {
/*
 * PURIFY sometimes reports thousands of Array Bounds Reads and Free Memory
 * Reads on the XGetPixel call on the next line. Appears to do this if image
 * loaded is small.
 */
				XSetForeground(display, fp->backGC, XGetPixel(fp->image, x, y));
			}
			if (fp->pointsize <= 1)
				XDrawPoint(display, fp->cache, fp->backGC, xp, yp);
			else
#ifndef NOTHREED_EFFECT
				XFillRectangle(display, fp->cache, fp->backGC, xp, yp,
					       fp->pointsize, fp->pointsize);
#else
			if (fp->pointsize < 6)
				XFillRectangle(display, fp->cache, fp->backGC, xp, yp,
					       fp->pointsize, fp->pointsize);
			else
				XFillArc(display, fp->cache, fp->backGC, xp, yp,
				  fp->pointsize, fp->pointsize, 0, 360 * 64);
#endif
		}
}

extern char *message;

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
			text1 = (char *) strdup("uname() failed");
		} else {
			char       *s;

			if ((s = strchr(uts.nodename, '.')))
				*s = 0;
			text1 = (char *) malloc(strlen(uts.nodename) +
						strlen(uts.sysname) +
						strlen(uts.release) + 10);
#ifdef AIXV3
			(void) sprintf(text1, "%s\n%s %s",
				       uts.nodename, uts.sysname, "3");
#else
			(void) sprintf(text1, "%s\n%s %s",
				     uts.nodename, uts.sysname, uts.release);
#endif
		}
#else
		text1 = (char *) strdup("OpenVMS");	/* It says release 0 in my utsname.h */
#endif
	} else {
		text1 = (char *) strdup(message);
	}
	while (*text1 && (text1[strlen(text1) - 1] == '\r' ||
			  text1[strlen(text1) - 1] == '\n'))
		text1[strlen(text1) - 1] = 0;
	text2 = (char *) strdup(text1);


	if (messagefont == None)
		messagefont = getFont(display);

	(void) memset(&overall, 0, sizeof (overall));
	token = text1;
	lines = 0;
	while ((line = strtok(token, "\r\n"))) {
		XCharStruct o2;
		int         ascent, descent, direction;

		token = 0;
		(void) XTextExtents(messagefont, line, strlen(line),
				    &direction, &ascent, &descent, &o2);
		overall.lbearing = MAX(overall.lbearing, o2.lbearing);
		overall.rbearing = MAX(overall.rbearing, o2.rbearing);
		lines++;
	}

	width = overall.lbearing + overall.rbearing + margin + margin + 1;
	height = ((messagefont->ascent + messagefont->descent) * lines) +
		margin + margin;

	text_pixmap = XCreatePixmap(display, MI_WINDOW(mi), width, height, 1);

	gcv.font = messagefont->fid;
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

		(void) XTextExtents(messagefont, line, strlen(line),
				    &direction, &ascent, &descent, &o2);
		xoff = ((overall.lbearing + overall.rbearing) -
			(o2.lbearing + o2.rbearing)) / 2;

		(void) XDrawString(display, text_pixmap, gc,
				   overall.lbearing + margin + xoff,
				   ((messagefont->ascent * (lines + 1)) +
				    (messagefont->descent * lines) + margin),
				   line, strlen(line));
		lines++;
	}
	(void) free((void *) text1);
	(void) free((void *) text2);
	/*XUnloadFont(display, messagefont->fid); */
	XFreeGC(display, gc);

	*image = XGetImage(display, text_pixmap, 0, 0, width, height,
			   1L, XYPixmap);
	XFreePixmap(display, text_pixmap);
}

static void
init_stuff(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	flagstruct *fp = &flags[MI_SCREEN(mi)];

	if (!fp->logo)
		getImage(mi, &fp->logo, FLAG_WIDTH, FLAG_HEIGHT, FLAG_BITS,
#if defined( USE_XPM ) || defined( USE_XPMINC )
			 DEFAULT_XPM, FLAG_NAME,
#endif
			 &fp->graphics_format, &fp->cmap, &fp->black);
#ifndef STANDALONE
	if (fp->cmap != None) {
		setColormap(display, window, fp->cmap, MI_IS_INWINDOW(mi));
		if (fp->backGC == None) {
			XGCValues   xgcv;

			xgcv.background = fp->black;
			fp->backGC = XCreateGC(display, window, GCBackground, &xgcv);
		}
	} else
#endif /* STANDALONE */
	{
		fp->black = MI_BLACK_PIXEL(mi);
		fp->backGC = MI_GC(mi);
	}
}

static void
free_stuff(Display * display, flagstruct * fp)
{
	if (fp->cmap != None) {
		XFreeColormap(display, fp->cmap);
		if (fp->backGC != None) {
			XFreeGC(display, fp->backGC);
			fp->backGC = None;
		}
		fp->cmap = None;
	} else
		fp->backGC = None;
	destroyImage(&fp->logo, &fp->graphics_format);
}

void
init_flag(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	int         size = MI_SIZE(mi);
	flagstruct *fp;

	if (flags == NULL) {
		if ((flags = (flagstruct *) calloc(MI_NUM_SCREENS(mi),
					       sizeof (flagstruct))) == NULL)
			return;
	}
	fp = &flags[MI_SCREEN(mi)];

	init_stuff(mi);

	fp->width = MI_WIDTH(mi);
	fp->height = MI_HEIGHT(mi);

	if (fp->image) {
		if (fp->graphics_format == IS_XBM)
			XFree(fp->image);
		else
			(void) XDestroyImage(fp->image);
		fp->image = NULL;
	}
	if (MI_IS_FULLRANDOM(mi) ||
	    (MI_BITMAP(mi) && *(MI_BITMAP(mi)) && message && *message) ||
	((!(MI_BITMAP(mi)) || !*(MI_BITMAP(mi))) && (!message || !*message)))
		fp->choice = (int) (LRAND() & 1);
	else if (MI_BITMAP(mi) && *(MI_BITMAP(mi)))
		fp->choice = IMAGE_FLAG;
	else
		fp->choice = MESSAGE_FLAG;
	if (fp->choice == MESSAGE_FLAG) {
		getText(mi, &fp->image);
	} else {
		int         depth = MI_DEPTH(mi);

		if (depth >= 24 && depth < 32)
			depth = 32;
		if (fp->graphics_format < IS_XPM)
			depth = 1;
		fp->image = XCreateImage(display, MI_VISUAL(mi), depth,
			 (fp->graphics_format < IS_XPM) ? XYBitmap : ZPixmap,
					 0, fp->logo->data,
					 (fp->graphics_format == IS_RASTERFILE && (fp->logo->width & 1)) ?
					 fp->logo->width + 1 : fp->logo->width, fp->logo->height, 8, 0);
		fp->image->byte_order = LSBFirst;
		fp->image->bitmap_bit_order = LSBFirst;
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

	if (fp->cache) {
		XFreePixmap(display, fp->cache);
	}
	if (!(fp->cache = XCreatePixmap(display, MI_WINDOW(mi),
					MAXW(fp), MAXH(fp), MI_DEPTH(mi)))) {
		return;
	}
	XSetForeground(display, fp->backGC, fp->black);
	XSetBackground(display, fp->backGC, fp->black);
	XFillRectangle(display, fp->cache, fp->backGC,
		       0, 0, MAXW(fp), MAXH(fp));
	/* don't want any exposure events from XCopyArea */
	XSetGraphicsExposures(display, fp->backGC, False);
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

	MI_CLEARWINDOWCOLORMAP(mi, fp->backGC, fp->black);
}

void
draw_flag(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	flagstruct *fp = &flags[MI_SCREEN(mi)];

	MI_IS_DRAWN(mi) = True;

	if (fp->width <= MAXW(fp) || fp->height <= MAXH(fp)) {
		fp->size = MININITSIZE;
		/* fp->pointsize = MINPOINTSIZE; */
		XCopyArea(display, fp->cache, window, fp->backGC,
			  0, 0, MINW(fp), MINH(fp), fp->x_flag, fp->y_flag);
	} else {
		if ((fp->size + fp->inctaille) > MAXSCALE)
			fp->inctaille = -fp->inctaille;
		if ((fp->size + fp->inctaille) < MINSCALE)
			fp->inctaille = -fp->inctaille;
		fp->size += fp->inctaille;
		XCopyArea(display, fp->cache, window, fp->backGC,
			  0, 0, MAXW(fp), MAXH(fp), fp->x_flag, fp->y_flag);
	}
	XSetForeground(MI_DISPLAY(mi), fp->backGC, fp->black);
	XFillRectangle(display, fp->cache, fp->backGC,
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
			Display    *display = MI_DISPLAY(mi);

			if (fp->cache)
				XFreePixmap(display, fp->cache);
			if (fp->image) {
				if (fp->choice == IMAGE_FLAG && (fp->graphics_format == IS_XBM ||
					  fp->graphics_format == IS_XBMDONE))
					XFree((caddr_t) fp->image);	/* Do not destroy data */
				else
					(void) XDestroyImage(fp->image);
			}
			free_stuff(display, fp);
		}
		(void) free((void *) flags);
		flags = NULL;
	}
	if (messagefont != None) {
		XFreeFont(MI_DISPLAY(mi), messagefont);
		messagefont = None;
	}
}

void
refresh_flag(ModeInfo * mi)
{
	flagstruct *fp = &flags[MI_SCREEN(mi)];

	MI_CLEARWINDOWCOLORMAP(mi, fp->backGC, fp->black);
#if defined( USE_XPM ) || defined( USE_XPMINC )
	/* This is needed when another program changes the colormap. */
	free_stuff(MI_DISPLAY(mi), fp);
	init_stuff(mi);
#endif
}

#endif /* MODE_flag */
