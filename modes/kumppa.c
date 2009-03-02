/* -*- Mode: C; tab-width: 4 -*- */
/* kumppa ---  */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)kumppa.c	4.11 98/06/11 xlockmore";

#endif

/*-
   Copyright (C) Teemu Suutari (temisu@utu.fi) Feb 1998

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the X Consortium shall
   not be used in advertising or otherwise to promote the sale, use or
   other dealings in this Software without prior written authorization
   from the X Consortium.

   * Revision History:
   * 16-Jul-98:  xlockmore version by Jouk Jansen <joukj@hrem.stm.tudelft.nl>
   * Feb 1998 :  original xscreensaver version by Teemu Suutari <temisu@utu.fi>
 */

/*-
   *** This is contest-version. Don't look any further, code is *very* ugly.
 */


#include <math.h>


#if 0
/* commented out since xlockmore does not support (yet) double buffering */
#ifdef HAVE_XDBE_EXTENSION
#include <X11/extensions/Xdbe.h>
#endif /* HAVE_XDBE_EXTENSION */
#endif

#ifdef STANDALONE
#define PROGCLASS "kumppa"
#define HACK_INIT init_kumppa
#define HACK_DRAW draw_kumppa
#define kumppa_opts xlockmore_opts
#define DEFAULTS "*delay: 0 \n" \
 "*cycles: 1000 \n" \
 ".background: black\n",\
 "*speed: 0.1",\
#include "xlockmore.h"		/* in xscreensaver distribution */
#else /* STANDALONE */
#include "xlock.h"		/* in xlockmore distribution */

#endif /* STANDALONE */

#ifdef MODE_kumppa

#define DEF_COSILINES "True"
#define DEF_SPEED "0.1"

#if 0
#ifdef HAVE_XDBE_EXTENSION
#define DEF_USEDOUBLE "False"

static Bool usedouble;

#endif /* HAVE_XDBE_EXTENSION */
#endif

static Bool cosilines;
static float speed;

static XrmOptionDescRec opts[] =
{
	{(char *) "-speed", (char *) ".kumppa.speed", XrmoptionSepArg, (caddr_t) NULL},
#if 0
#ifdef HAVE_XDBE_EXTENSION
	{(char *) "-dbuf", (char *) ".kumppa.dbuf", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+dbuf", (char *) ".kumppa.dbuf", XrmoptionNoArg, (caddr_t) "off"},
#endif				/* HAVE_XDBE_EXTENSION */
#endif
	{(char *) "-rrandom", (char *) ".kumppa.rrandom", XrmoptionNoArg, (caddr_t) "on"},
	{(char *) "+rrandom", (char *) ".kumppa.rrandom", XrmoptionNoArg, (caddr_t) "off"}
};

static argtype vars[] =
{
	{(caddr_t *) & speed, (char *) "speed", (char *) "speed", (char *) DEF_SPEED, t_Float},
#if 0
#ifdef HAVE_XDBE_EXTENSION
	{(caddr_t *) & usedouble, (char *) "dbuf", (char *) "dbuf", (char *) DEF_USEDOUBLE, t_Bool},
#endif				/* HAVE_XDBE_EXTENSION */
#endif
	{(caddr_t *) & cosilines, (char *) "rrandom", (char *) "rrandom", (char *) DEF_COSILINES, t_Bool}
};
static OptionStruct desc[] =
{
	{(char *) "-speed num", (char *) "Speed"},
#if 0
#ifdef HAVE_XDBE_EXTENSION
	{(char *) "-/+dbuf", (char *) "turn on/off double buffering"},
#endif				/* HAVE_XDBE_EXTENSION */
#endif
	{(char *) "-/+rrandom", (char *) "turn on/off random"}
};

ModeSpecOpt kumppa_opts =
{sizeof opts / sizeof opts[0], opts, sizeof vars / sizeof vars[0], vars, desc};

#ifdef USE_MODULES
ModStruct   kumppa_description =
{"kumppa", "init_kumppa", "draw_kumppa", "release_kumppa",
 "refresh_kumppa", "init_kumppa", NULL, &kumppa_opts,
 10000, 1, 1000, 1, 64, 1.0, "",
 "Shows Kumppa", 0, NULL};

#endif

static const unsigned char colors[96] =
{0, 0, 255, 0, 51, 255, 0, 102, 255, 0, 153, 255, 0, 204, 255,
 0, 255, 255, 0, 255, 204, 0, 255, 153, 0, 255, 102, 0, 255, 51,
 0, 255, 0, 51, 255, 0, 102, 255, 0, 153, 255, 0, 204, 255, 0,
 255, 255, 0, 255, 204, 0, 255, 153, 0, 255, 102, 0, 255, 51, 0,
 255, 0, 0, 255, 0, 51, 255, 0, 102, 255, 0, 153, 255, 0, 204,
 255, 0, 255, 219, 0, 255, 182, 0, 255, 146, 0, 255, 109, 0, 255,
 73, 0, 255, 37, 0, 255};

static const float cosinus[8][6] =
{
	{-0.07, 0.12, -0.06, 32, 25, 37},
	{0.08, -0.03, 0.05, 51, 46, 32},
	{0.12, 0.07, -0.13, 27, 45, 36},
	{0.05, -0.04, -0.07, 36, 27, 39},
	{-0.02, -0.07, 0.1, 21, 43, 42},
	{-0.11, 0.06, 0.02, 51, 25, 34},
	{0.04, -0.15, 0.02, 42, 32, 25},
	{-0.02, -0.04, -0.13, 34, 20, 15}};

static const float acosinus[24] =
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const int   ocoords[8] =
{0, 0, 0, 0, 0, 0, 0, 0};

typedef struct {
	Colormap    cmap;
	float      *acosinus;
	unsigned long blackpixel, whitepixel, fg, bg;
	int         coords[8];
	int         ocoords[8];

	GC          fgc[33];
	GC          cgc;
	int         sizx, sizy;
	int         midx, midy;

	int        *Xrotations;
	int        *Yrotations;
	int        *Xrottable;
	int        *Yrottable;

	int        *rotateX;
	int        *rotateY;

	int         rotsizeX, rotsizeY;
	int         stateX, stateY;
	int         dir, time;
	int         rx, ry;
	int         c;
	long        c1;
	Bool        cosilines;
} kumppastruct;

static kumppastruct *kumppas = NULL;



static int
Satnum(int maxi)
{
	return (int) (maxi * ((double) NRAND(2500) / 2500.0));
}


static void
palaRotate(ModeInfo * mi, int x, int y)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	int         ax, ay, bx, by, cx, cy;
	kumppastruct *s = &kumppas[MI_SCREEN(mi)];

	ax = s->rotateX[x];
	ay = s->rotateY[y];
	bx = s->rotateX[x + 1] + 2;
	by = s->rotateY[y + 1] + 2;
	cx = s->rotateX[x] + (x - s->rx) - s->dir * (y - s->ry);
	cy = s->rotateY[y] + s->dir * (x - s->rx) + (y - s->ry);
	if (cx < 0) {
		ax -= cx;
		cx = 0;
	}
	if (cy < 0) {
		ay -= cy;
		cy = 0;
	}
	if (cx + bx - ax > s->sizx)
		bx = ax - cx + s->sizx;
	if (cy + by - ay > s->sizy)
		by = ay - cy + s->sizy;
	if (ax < bx && ay < by) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XCopyArea(display, window, window, s->cgc, ax, ay, bx - ax, by - ay, cx, cy);
		} else {
			XCopyArea(display, window, window, MI_GC(mi), ax, ay, bx - ax, by - ay, cx, cy);
		}
	}
}


static void
rotate(ModeInfo * mi)
{
	int         x, y;
	int         dx, dy;
	kumppastruct *s = &kumppas[MI_SCREEN(mi)];

	s->rx = s->Xrottable[s->stateX + 1] - s->Xrottable[s->stateX];
	s->ry = s->Yrottable[s->stateX + 1] - s->Yrottable[s->stateY];


	for (x = 0; x <= s->rx; x++)
		s->rotateX[x] = (x) ? s->midx - 1 - s->Xrotations[s->Xrottable[s->stateX + 1] - x] : 0;
	for (x = 0; x <= s->rx; x++)
		s->rotateX[x + s->rx + 1] = (x == s->rx) ? s->sizx - 1 : s->midx + s->Xrotations[s->Xrottable[s->stateX] + x];
	for (y = 0; y <= s->ry; y++)
		s->rotateY[y] = (y) ? s->midy - 1 - s->Yrotations[s->Yrottable[s->stateY + 1] - y] : 0;
	for (y = 0; y <= s->ry; y++)
		s->rotateY[y + s->ry + 1] = (y == s->ry) ? s->sizy - 1 : s->midy + s->Yrotations[s->Yrottable[s->stateY] + y];

	x = (s->rx > s->ry) ? s->rx : s->ry;
	for (dy = 0; dy < (x + 1) << 1; dy++)
		for (dx = 0; dx < (x + 1) << 1; dx++) {
			y = (s->rx > s->ry) ? s->ry - s->rx : 0;
			if (dy + y >= 0 && dy < (s->ry + 1) << 1 && dx < (s->rx + 1) << 1)
				if (dy + y + dx <= s->ry + s->rx && dy + y - dx <= s->ry - s->rx) {
					palaRotate(mi, (s->rx << 1) + 1 - dx, dy + y);
					palaRotate(mi, dx, (s->ry << 1) + 1 - dy - y);
				}
			y = (s->ry > s->rx) ? s->rx - s->ry : 0;
			if (dy + y >= 0 && dx < (s->ry + 1) << 1 && dy < (s->rx + 1) << 1)
				if (dy + y + dx <= s->ry + s->rx && dx - dy - y >= s->ry - s->rx) {
					palaRotate(mi, dy + y, dx);
					palaRotate(mi, (s->rx << 1) + 1 - dy - y, (s->ry << 1) + 1 - dx);
				}
		}
	s->stateX++;
	if (s->stateX == s->rotsizeX)
		s->stateX = 0;
	s->stateY++;
	if (s->stateY == s->rotsizeY)
		s->stateY = 0;
}



static void
make_rots(ModeInfo * mi, double xspeed, double yspeed)
{
	int         a, b, c, f, g, j, k = 0, l;
	double      m, om, ok;
	double      d, ix, iy;
	int         maxi;
	kumppastruct *s = &kumppas[MI_SCREEN(mi)];

	Bool       *chks;

	s->rotsizeX = (int) (2 / xspeed + 1);
	ix = (double) (s->midx + 1) / (double) (s->rotsizeX);
	s->rotsizeY = (int) (2 / yspeed + 1);
	iy = (double) (s->midy + 1) / (double) (s->rotsizeY);

	if (s->Xrotations)
		(void) free((void *) s->Xrotations);
	if (s->Yrotations)
		(void) free((void *) s->Yrotations);

	s->Xrotations = (int *) calloc((s->midx + 2), sizeof (int));
	s->Yrotations = (int *) calloc((s->midy + 2), sizeof (int));

	if (s->Xrottable)
		(void) free((void *) s->Xrottable);
	if (s->Yrottable)
		(void) free((void *) s->Yrottable);
	s->Xrottable = (int *) malloc((s->rotsizeX + 1) * sizeof (int));
	s->Yrottable = (int *) malloc((s->rotsizeY + 1) * sizeof (int));

	chks = (Bool *) malloc(((s->midx > s->midy) ? s->midx : s->midy) * sizeof (Bool));


	maxi = 0;
	c = 0;
	d = 0;
	g = 0;
	for (a = 0; a < s->midx; a++)
		chks[a] = True;
	for (a = 0; a < s->rotsizeX; a++) {
		s->Xrottable[a] = c;
		f = (int) (d + ix) - g;		/*viivojen lkm. */
		g += f;
		if (g > s->midx) {
			f -= g - s->midx;
			g = s->midx;
		}
		for (b = 0; b < f; b++) {
			m = 0;
			for (j = 0; j < s->midx; j++) {		/*testi */
				if (chks[j]) {
					om = 0;
					ok = 1;
					l = 0;
					while (j + l < s->midx && om + 12 * ok > m) {
						if (j - l >= 0) {
							if (chks[j - l])
								om += ok;
						} else if (chks[l - j])
							om += ok;
						if (chks[j + l])
							om += ok;
						ok /= 1.5;
						l++;
					}
					if (om >= m) {
						k = j;
						m = om;
					}
				}
			}
			chks[k] = False;
			l = c;
			while (l >= s->Xrottable[a]) {
				if (l != s->Xrottable[a])
					s->Xrotations[l] = s->Xrotations[l - 1];
				if (k > s->Xrotations[l] || l == s->Xrottable[a]) {
					s->Xrotations[l] = k;
					c++;
					l = s->Xrottable[a];
				}
				l--;
			}
		}
		d += ix;
		if (maxi < c - s->Xrottable[a])
			maxi = c - s->Xrottable[a];
	}
	s->Xrottable[a] = c;
	if (s->rotateX)
		(void) free((void *) s->rotateX);
	s->rotateX = (int *) calloc((maxi + 2) << 1, sizeof (int));

	maxi = 0;
	c = 0;
	d = 0;
	g = 0;
	for (a = 0; a < s->midy; a++)
		chks[a] = True;
	for (a = 0; a < s->rotsizeY; a++) {
		s->Yrottable[a] = c;
		f = (int) (d + iy) - g;		/*viivojen lkm. */
		g += f;
		if (g > s->midy) {
			f -= g - s->midy;
			g = s->midy;
		}
		for (b = 0; b < f; b++) {
			m = 0;
			for (j = 0; j < s->midy; j++) {		/*testi */
				if (chks[j]) {
					om = 0;
					ok = 1;
					l = 0;
					while (j + l < s->midy && om + 12 * ok > m) {
						if (j - l >= 0) {
							if (chks[j - l])
								om += ok;
						} else if (chks[l - j])
							om += ok;
						if (chks[j + l])
							om += ok;
						ok /= 1.5;
						l++;
					}
					if (om >= m) {
						k = j;
						m = om;
					}
				}
			}
			chks[k] = False;
			l = c;
			while (l >= s->Yrottable[a]) {
				if (l != s->Yrottable[a])
					s->Yrotations[l] = s->Yrotations[l - 1];
				if (k > s->Yrotations[l] || l == s->Yrottable[a]) {
					s->Yrotations[l] = k;
					c++;
					l = s->Yrottable[a];
				}
				l--;
			}

		}
		d += iy;
		if (maxi < c - s->Yrottable[a])
			maxi = c - s->Yrottable[a];
	}
	s->Yrottable[a] = c;
	if (s->rotateY)
		(void) free((void *) s->rotateY);
	s->rotateY = (int *) calloc((maxi + 2) << 1, sizeof (int));

	(void) free((void *) chks);
}


void
init_kumppa(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	XGCValues   xgcv;
	int         n, i;
	double      rspeed;
	kumppastruct *s;

	if (kumppas == NULL) {
		if ((kumppas = (kumppastruct *) calloc(MI_NUM_SCREENS(mi),
					     sizeof (kumppastruct))) == NULL)
			return;
	}
	s = &kumppas[MI_SCREEN(mi)];

	if (!s->acosinus) {
		if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
			XColor      color;

#ifndef STANDALONE
			extern char *background;
			extern char *foreground;

			s->fg = MI_FG_PIXEL(mi);
			s->bg = MI_BG_PIXEL(mi);
#endif
			s->blackpixel = MI_BLACK_PIXEL(mi);
			s->whitepixel = MI_WHITE_PIXEL(mi);
			s->cmap = XCreateColormap(display, window, MI_VISUAL(mi), AllocNone);

			XSetWindowColormap(display, window, s->cmap);
			(void) XParseColor(display, s->cmap, "black", &color);
			(void) XAllocColor(display, s->cmap, &color);
			MI_BLACK_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, s->cmap, "white", &color);
			(void) XAllocColor(display, s->cmap, &color);
			MI_WHITE_PIXEL(mi) = color.pixel;
#ifndef STANDALONE
			(void) XParseColor(display, s->cmap, background, &color);
			(void) XAllocColor(display, s->cmap, &color);
			MI_BG_PIXEL(mi) = color.pixel;
			(void) XParseColor(display, s->cmap, foreground, &color);
			(void) XAllocColor(display, s->cmap, &color);
			MI_FG_PIXEL(mi) = color.pixel;
#endif

			xgcv.function = GXcopy;
			xgcv.foreground = MI_BLACK_PIXEL(mi);
			s->fgc[32] = XCreateGC(display, window, GCForeground | GCFunction, &xgcv);

			n = 0;
#if 0
			if (mono_p) {
				s->fgc[0] = s->fgc[32];
				xgcv.foreground = MI_BLACK_PIXEL(mi);
				s->fgc[1] = XCreateGC(display, window, GCForeground | GCFunction, &xgcv);
				for (i = 0; i < 32; i += 2)
					s->fgc[i] = s->fgc[0];
				for (i = 1; i < 32; i += 2)
					s->fgc[i] = s->fgc[1];
			} else
#endif
				for (i = 0; i < 32; i++) {
					color.red = colors[n++] * 256;
					color.green = colors[n++] * 256;
					color.blue = colors[n++] * 256;
					color.flags = DoRed | DoGreen | DoBlue;
					(void) XAllocColor(display, s->cmap, &color);
					xgcv.foreground = color.pixel;
					s->fgc[i] = XCreateGC(display, window, GCForeground | GCFunction, &xgcv);
				}
			xgcv.foreground = MI_BLACK_PIXEL(mi);
			xgcv.function = GXcopy;
			s->cgc = XCreateGC(display, window, GCForeground | GCFunction, &xgcv);
		}
		s->acosinus = (float *) malloc(24 * sizeof (float));
		(void) memcpy(s->acosinus, acosinus, 24 * sizeof (float));
		(void) memcpy(s->ocoords, ocoords, 8 * sizeof (int));
	}

	if (MI_IS_FULLRANDOM(mi)) {
		if (NRAND(2) == 1)
			s->cosilines = False;
		else
			s->cosilines = True;
	} else {
		s->cosilines = cosilines;
	}
	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		XInstallColormap(display, s->cmap);
		XSetGraphicsExposures(display, s->cgc, False);
	} else {
		XSetGraphicsExposures(display, MI_GC(mi), False);
		s->c1 = NRAND(MI_NPIXELS(mi));
	}
	s->time = MI_CYCLES(mi) + NRAND(MI_CYCLES(mi));

#if 0
#ifdef HAVE_XDBE_EXTENSION
	if (get_string_resource("dbuf", "String") != NULL && get_string_resource("dbuf", "String")[0] != 0)
		usedouble = True;
	if (usedouble) {
		XdbeQueryExtension(display, &n, &i);
		if (n == 0 && i == 0) {
			(void) fprintf(stderr, "Double buffer extension not supported!\n");
			usedouble = False;
		}
	}
	if (usedouble)
		win[1] = XdbeAllocateBackBufferName(display, win[0], XdbeUndefined);
#endif /* HAVE_XDBE_EXTENSION */
#endif

	rspeed = (double) speed;
	if (rspeed < 0.0001 || rspeed > 0.2) {
		(void) fprintf(stderr,
		   "Speed not in valid range! (0.0001 - 0.2), using 0.1 \n");
		rspeed = 0.1;
	}
	s->sizx = MI_WIDTH(mi);
	s->sizy = MI_HEIGHT(mi);
	s->midx = s->sizx >> 1;
	s->midy = s->sizy >> 1;
	s->stateX = 0;
	s->stateY = 0;
	s->c = 0;
	s->dir = (LRAND() & 1) ? -1 : 1;
	MI_CLEARWINDOW(mi);

	make_rots(mi, rspeed, rspeed);
}


void
draw_kumppa(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	GC          gc;
	kumppastruct *s = &kumppas[MI_SCREEN(mi)];

#if 0
#ifdef HAVE_XDBE_EXTENSION
	XdbeSwapInfo xdswp;

#endif /* HAVE_XDBE_EXTENSION */
#endif
	int         a, b, e;
	float       f;

	MI_IS_DRAWN(mi) = True;


#if 0
#ifdef HAVE_XDBE_EXTENSION
	if (usedouble) {
		xdswp.swap_action = XdbeUndefined;
		xdswp.swap_window = win[0];
	} else
#endif /* HAVE_XDBE_EXTENSION */
		win[1] = win[0];
#endif

	if (s->cosilines) {
		s->c++;
		for (a = 0; a < 8; a++) {
			f = 0;
			for (b = 0; b < 3; b++) {
				s->acosinus[a * 3 + b] += cosinus[a][b];
				f += cosinus[a][b + 3] *
					sin((double) s->acosinus[a * 3 + b]);
			}
			s->coords[a] = (int) f;
		}
		for (a = 0; a < 4; a++) {
			if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
#if 0
				gc = (mono_p) ? s->fgc[1] : s->fgc[((a << 2) + s->c) & 31];
#endif
				gc = s->fgc[((a << 2) + s->c) & 31];
			} else {
				gc = MI_GC(mi);
				if (MI_NPIXELS(mi) <= 2)
					XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
				else
					XSetForeground(display, gc, MI_PIXEL(mi, ((((a << 2) + s->c) & 31) * MI_NPIXELS(mi) / 32) % MI_NPIXELS(mi)));
			}
			XDrawLine(display, window, gc,
				  s->midx + s->ocoords[a << 1], s->midy + s->ocoords[(a << 1) + 1],
				  s->midx + s->coords[a << 1], s->midy + s->coords[(a << 1) + 1]);
			s->ocoords[a << 1] = s->coords[a << 1];
			s->ocoords[(a << 1) + 1] = s->coords[(a << 1) + 1];
		}

	} else {
		for (e = 0; e < 8; e++) {
			a = Satnum(50);
			if (a >= 32)
				a = 32;
			b = Satnum(32) - 16 + s->midx;
			s->c = Satnum(32) - 16 + s->midy;
			if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
				gc = s->fgc[a];
			} else {
				gc = MI_GC(mi);
				if (a == 32)
					XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				else if (MI_NPIXELS(mi) > 2)
					XSetForeground(display, gc, MI_PIXEL(mi, (a * MI_NPIXELS(mi) / 32) % MI_NPIXELS(mi)));
				else if (a == 0)
					XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
				else
					XSetForeground(display, gc, MI_WHITE_PIXEL(mi));
			}
			XFillRectangle(display, window, gc, b, s->c, 2, 2);
		}
	}
	if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
		gc = s->fgc[32];
	} else {
		gc = MI_GC(mi);
		XSetForeground(display, gc, MI_BLACK_PIXEL(mi));
	}
	XFillRectangle(display, window, gc, s->midx - 2, s->midy - 2, 4, 4);
	rotate(mi);
#if 0
#ifdef HAVE_XDBE_EXTENSION
	if (usedouble)
		XdbeSwapBuffers(display, &xdswp, 1);
#endif /* HAVE_XDBE_EXTENSION */
#endif
	if (--s->time <= 0) {
		s->time = MI_CYCLES(mi) + NRAND(MI_CYCLES(mi));
		s->dir = s->dir * (-1);
	}
}


void
refresh_kumppa(ModeInfo * mi)
{
	init_kumppa(mi);
}

void
release_kumppa(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);

	if (kumppas != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			kumppastruct *s = &kumppas[screen];

			if (MI_IS_INSTALL(mi) && MI_NPIXELS(mi) > 2) {
				int i;

				MI_WHITE_PIXEL(mi) = s->whitepixel;
				MI_BLACK_PIXEL(mi) = s->blackpixel;
#ifndef STANDALONE
				MI_FG_PIXEL(mi) = s->fg;
				MI_BG_PIXEL(mi) = s->bg;
#endif
				if (s->fgc[32])
					XFreeGC(display, s->fgc[32]);
#if 0
				if (mono_p) {
					if (s->fgc[1])
						XFreeGC(display, s->fgc[1]);
				} else
#endif
				for (i = 0; i < 32; i++) {
					if (s->fgc[i])
						XFreeGC(display, s->fgc[i]);
				}
				if (s->cgc)
					XFreeGC(display, s->cgc);
				if (s->cmap) {
					XFreeColormap(display, s->cmap);
				}
			}
			if (s->acosinus)
				(void) free((void *) s->acosinus);
			if (s->Xrotations)
				(void) free((void *) s->Xrotations);
			if (s->Yrotations)
				(void) free((void *) s->Yrotations);
			if (s->Xrottable)
				(void) free((void *) s->Xrottable);
			if (s->Yrottable)
				(void) free((void *) s->Yrottable);
			if (s->rotateX)
				(void) free((void *) s->rotateX);
			if (s->rotateY)
				(void) free((void *) s->rotateY);
		}
		(void) free((void *) kumppas);
		kumppas = NULL;
	}
}

#endif /* MODE_kumppa */
