/* -*- Mode: C; tab-width: 4 -*-
 * flag --- a waving flag
 */
#if 0
static const char sccsid[] = "@(#)flag.c	4.02 97/04/01 xlockmore";
#endif

/* Copyright (c) 1996 Charles Vidal <vidalc@univ-mlv.fr>.
 * PEtite demo X11 de charles vidal 15 05 96
 * tourne sous Linux et SOLARIS
 * thank's to Bas van Gaalen, Holland, PD, for his sources
 * in pascal vous devez rajouter une ligne dans mode.c
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
 * 22-Jan-98: jwz: made the flag wigglier; added xpm support.
 *            (I tried to do this by re-porting from xlockmore, but the
 *            current xlockmore version is completely inscrutable.)
 * 13-May-97: jwz@jwz.org: turned into a standalone program.
 *			  Made it able to animate arbitrary (runtime) text or bitmaps.
 * 01-May-96: written.
 */

#ifdef HAVE_COCOA
# define DEF_FONT "Monaco 15"
#else
# define DEF_FONT "fixed"
#endif

#ifdef STANDALONE
# define DEFAULTS	"*delay:		50000   \n"		\
					"*cycles:		1000    \n"		\
					"*size:			-7      \n"		\
					"*ncolors:		200     \n"		\
					"*bitmap:				\n"		\
					"*font:		" DEF_FONT	"\n"	\
					"*text:					\n"
# define BRIGHT_COLORS
# define UNIFORM_COLORS
# define reshape_flag 0
# define flag_handle_event 0
# include "xlockmore.h"				/* from the xscreensaver distribution */

#include "xpm-pixmap.h"
#include "images/bob.xbm"

#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
# include "flag.h"
#endif /* !STANDALONE */


#ifdef HAVE_UNAME
# include <sys/utsname.h>
#endif /* HAVE_UNAME */

#ifdef STANDALONE
static XrmOptionDescRec opts[] =
{
  { "-bitmap", ".flag.bitmap", XrmoptionSepArg, 0 },
  { "-text",   ".flag.text",   XrmoptionSepArg, 0 }
};

#endif /* STANDALONE */

ENTRYPOINT ModeSpecOpt flag_opts = {
#ifdef STANDALONE
  2, opts, 0, NULL, NULL
#else  /* !STANDALONE */
  0, NULL, 0, NULL, NULL
#endif /* STANDALONE */
};

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
    Bool		dbufp;
	Pixmap      cache;
	int         width, height;
	int         pointsize;
	float      size;
	float      inctaille;
	int         startcolor;
    XImage     *image;
} flagstruct;

static flagstruct *flags = NULL;

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
		for (y = fp->image->height-1; y >= 0; y--) {
			xp = (int) (fp->size * (float) x) +
				fp->stab[(fp->sidx + x + y) % ANGLES];
			yp = (int) (fp->size * (float) y) +
				fp->stab[(fp->sidx + 4 * x + y + y) % ANGLES];

			if (fp->image->depth > 1)
			  XSetForeground(display, MI_GC(mi),
							 XGetPixel(fp->image, x, y));
			else if (XGetPixel(fp->image, x, y))
				XSetForeground(display, MI_GC(mi), MI_WIN_BLACK_PIXEL(mi));
			else if (MI_NPIXELS(mi) <= 2)
				XSetForeground(display, MI_GC(mi), MI_WIN_WHITE_PIXEL(mi));
			else
				XSetForeground(display, MI_GC(mi),
					       MI_PIXEL(mi, (y + x + fp->sidx + fp->startcolor) % MI_NPIXELS(mi)));

            if (fp->cache == MI_WINDOW(mi)) {  /* not double-buffering */
              xp += fp->x_flag;
              yp += fp->y_flag;
            }

			if (fp->pointsize <= 1)
				XDrawPoint(display, fp->cache, MI_GC(mi), xp, yp);
			else if (fp->pointsize < 6)
				XFillRectangle(display, fp->cache, MI_GC(mi), xp, yp,
							   fp->pointsize, fp->pointsize);
			else
				XFillArc(display, fp->cache, MI_GC(mi), xp, yp,
						 fp->pointsize, fp->pointsize, 0, 360*64);
		}
}

#ifdef STANDALONE

static void
make_flag_bits(ModeInfo *mi)
{
  Display *dpy = MI_DISPLAY(mi);
  flagstruct *fp = &flags[MI_SCREEN(mi)];
  char *bitmap_name = get_string_resource (dpy, "bitmap", "Bitmap");
  char *text = get_string_resource (dpy, "text", "Text");

#ifdef HAVE_COCOA
  bitmap_name = 0;  /* #### always use default */
#endif

  /* If neither a bitmap nor text are specified, randomly select either
	 the builtin bitmap or builtin text. */
  if ((!bitmap_name || !*bitmap_name) && (!text || !*text))
	{
	  if (random() & 1)
		{
		  free(bitmap_name);
		  bitmap_name = strdup("(default)");
		}
	  else
		{
		  free(text);
		  text = strdup("(default)");
		}
	}

  if (bitmap_name &&
	  *bitmap_name &&
	  !!strcmp(bitmap_name, "(default)"))
	{
	  Pixmap bitmap = 0;
      int width = 0;
      int height = 0;

      bitmap = xpm_file_to_pixmap (dpy, MI_WINDOW (mi), bitmap_name,
                                   &width, &height, 0);
	  if (bitmap)
		{
		  fp->image = XGetImage(dpy, bitmap, 0, 0, width, height, ~0L,
								ZPixmap);
		  XFreePixmap(dpy, bitmap);
		}
	}
  else if (text && *text)
	{
	  char *text2;
	  char *fn = get_string_resource (dpy, "font", "Font");
	  char *def_fn = "fixed";
	  char *line, *token;
	  int width, height;
	  int lines;
	  int margin = 2;
	  int fg = 1;
	  int bg = 0;
	  Pixmap bitmap;
	  XFontStruct *font;
	  XCharStruct overall;
      XGCValues gcv;
	  GC gc;

	  if (!strcmp(text, "(default)"))
		{
# ifdef HAVE_UNAME
		  struct utsname uts;
		  if (uname (&uts) < 0)
			{
			  text = strdup("uname() failed");
			}
		  else
			{
			  char *s;
			  if ((s = strchr(uts.nodename, '.')))
				*s = 0;
			  text = (char *) malloc(strlen(uts.nodename) +
									 strlen(uts.sysname) +
									 strlen(uts.version) +
									 strlen(uts.release) + 10);
# ifdef _AIX
			  sprintf(text, "%s\n%s %s.%s",
					  uts.nodename, uts.sysname, uts.version, uts.release);
# else  /* !_AIX */
			  sprintf(text, "%s\n%s %s",
					  uts.nodename, uts.sysname, uts.release);
# endif /* !_AIX */
			}
#else	/* !HAVE_UNAME */
# ifdef VMS
		  text = strdup(getenv("SYS$NODE"));
# else
		  text = strdup("X\nScreen\nSaver");
# endif
#endif	/* !HAVE_UNAME */
		}

	  while (*text &&
			 (text[strlen(text)-1] == '\r' ||
			  text[strlen(text)-1] == '\n'))
		text[strlen(text)-1] = 0;

	  text2 = strdup(text);

	  if (!fn) fn = def_fn;
      font = XLoadQueryFont (dpy, fn);
      if (! font)
		{
		  fprintf(stderr, "%s: unable to load font %s; using %s\n",
				  progname, fn, def_fn);
		  font = XLoadQueryFont (dpy, def_fn);
		}

	  memset(&overall, 0, sizeof(overall));
	  token = text;
	  lines = 0;
	  while ((line = strtok(token, "\r\n")))
		{
		  XCharStruct o2;
		  int ascent, descent, direction;
		  token = 0;
		  XTextExtents(font, line, strlen(line),
					   &direction, &ascent, &descent, &o2);
		  overall.lbearing = MAX(overall.lbearing, o2.lbearing);
		  overall.rbearing = MAX(overall.rbearing, o2.rbearing);
		  lines++;
		}

	  width = overall.lbearing + overall.rbearing + margin + margin + 1;
	  height = ((font->ascent + font->descent) * lines) + margin + margin;

	  bitmap = XCreatePixmap(dpy, MI_WINDOW(mi), width, height, 1);

      gcv.font = font->fid;
      gcv.foreground = bg;
      gc = XCreateGC (dpy, bitmap, (GCFont | GCForeground), &gcv);
	  XFillRectangle(dpy, bitmap, gc, 0, 0, width, height);
	  XSetForeground(dpy, gc, fg);

	  token = text2;
	  lines = 0;
	  while ((line = strtok(token, "\r\n")))
		{
		  XCharStruct o2;
		  int ascent, descent, direction, xoff;
		  token = 0;

		  XTextExtents(font, line, strlen(line),
					   &direction, &ascent, &descent, &o2);
		  xoff = ((overall.lbearing + overall.rbearing) -
				  (o2.lbearing + o2.rbearing)) / 2;

		  XDrawString(dpy, bitmap, gc,
					  overall.lbearing + margin + xoff,
					  ((font->ascent * (lines + 1)) +
					   (font->descent * lines) +
					   margin),
					  line, strlen(line));
		  lines++;
		}
	  free(text2);
	  XUnloadFont(dpy, font->fid);
	  XFree((XPointer) font);
	  XFreeGC(dpy, gc);

	  fp->image = XGetImage(dpy, bitmap, 0, 0, width, height, 1L, XYPixmap);
	  XFreePixmap(dpy, bitmap);
	}
  else
	{
      char *bits = (char *) malloc (sizeof(bob_bits));
      memcpy (bits, bob_bits, sizeof(bob_bits));
	  fp->image = XCreateImage (dpy, MI_VISUAL(mi), 1, XYBitmap, 0,
								bits, bob_width, bob_height,
								8, 0);
	  fp->image->byte_order = LSBFirst;
	  fp->image->bitmap_bit_order = LSBFirst;
	}

  if (bitmap_name)
	free (bitmap_name);
  if (text)
	free (text);
}

#else  /* !STANDALONE */

static void
make_flag_bits(ModeInfo *mi)
{
  flagstruct *fp = &flags[MI_SCREEN(mi)];
  int x, y;
  int w = flag_width;
  int h = flag_height;
  int i = 0;
  fp->image =
	XCreateImage(MI_DISPLAY(mi), MI_VISUAL(mi),
				 1, XYBitmap, 0,					/* dpth, fmt, offset */
				 (char *) calloc ((w+8) / 8, h),	/* data */
				 w, h, 8, 0);						/* w, h, pad, bpl */
  /* Geez, what kinda goofy bit order is this?? */
  for (x = 0; x < w; x++)
	for (y = h-1; y >= 0; y--)
	  XPutPixel (fp->image, x, y, flag_bits[i++]);
}

#endif /* !STANDALONE */


ENTRYPOINT void
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

	make_flag_bits(mi);

	fp->width = MI_WIN_WIDTH(mi);
	fp->height = MI_WIN_HEIGHT(mi);

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
      fp->dbufp = True;
# ifdef HAVE_COCOA		/* Don't second-guess Quartz's double-buffering */
      fp->dbufp = False;
#endif
		fp->initialized = True;
		if (!fp->dbufp)
          fp->cache = MI_WINDOW(mi);  /* not double-buffering */
        else
          if (!(fp->cache = XCreatePixmap(display, MI_WINDOW(mi),
                                          MAXW(fp), MAXH(fp),
                                          MI_WIN_DEPTH(mi))))
#ifdef STANDALONE
		  exit(-1);
#else   /* !STANDALONE */
			error("%s: catastrophe memoire\n");
#endif /* !STANDALONE */
	}
	XSetForeground(display, MI_GC(mi), MI_WIN_BLACK_PIXEL(mi));
	XFillRectangle(display, fp->cache, MI_GC(mi),
		       0, 0, MAXW(fp), MAXH(fp));
	/* don't want any exposure events from XCopyArea */
	XSetGraphicsExposures(display, MI_GC(mi), False);
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

ENTRYPOINT void release_flag(ModeInfo * mi);


ENTRYPOINT void
draw_flag(ModeInfo * mi)
{
	Display    *display = MI_DISPLAY(mi);
	Window      window = MI_WINDOW(mi);
	flagstruct *fp = &flags[MI_SCREEN(mi)];

    if (fp->cache == window) {  /* not double-buffering */
      XClearWindow (display, window);
    } else if (fp->width <= MAXW(fp) || fp->height <= MAXH(fp)) {
		fp->size = MININITSIZE;
		/* fp->pointsize = MINPOINTSIZE; */
        XCopyArea(display, fp->cache, window, MI_GC(mi),
                  0, 0, MINW(fp), MINH(fp), fp->x_flag, fp->y_flag);
	} else {
		if ((fp->size + fp->inctaille) > MAXSCALE)
			fp->inctaille = -fp->inctaille;
		if ((fp->size + fp->inctaille) < MINSCALE)
			fp->inctaille = -fp->inctaille;
		fp->size += fp->inctaille;
        XCopyArea(display, fp->cache, window, MI_GC(mi),
                  0, 0, MAXW(fp), MAXH(fp), fp->x_flag, fp->y_flag);
	}
	XSetForeground(MI_DISPLAY(mi), MI_GC(mi), MI_WIN_BLACK_PIXEL(mi));
	XFillRectangle(display, fp->cache, MI_GC(mi),
		       0, 0, MAXW(fp), MAXH(fp));
	affiche(mi);
	fp->sidx += 2;
	fp->sidx %= (ANGLES * MI_NPIXELS(mi));
	fp->timer++;
	if ((MI_CYCLES(mi) > 0) && (fp->timer >= MI_CYCLES(mi)))
      {
        release_flag(mi);
		init_flag(mi);
      }
}

ENTRYPOINT void
release_flag(ModeInfo * mi)
{
	if (flags != NULL) {
		int         screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++)
		  {
			if (flags[screen].cache && flags[screen].dbufp)
				XFreePixmap(MI_DISPLAY(mi), flags[screen].cache);
			if (flags[screen].image)
			  XDestroyImage(flags[screen].image);
		  }
		(void) free((void *) flags);
		flags = NULL;
	}
}

ENTRYPOINT void
refresh_flag(ModeInfo * mi)
{
	/* Do nothing, it will refresh by itself */
}

XSCREENSAVER_MODULE ("Flag", flag)
