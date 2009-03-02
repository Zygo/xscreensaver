/* -*- Mode: C; tab-width: 4 -*- */
/* xjack -- Jack having one of those days */

#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)xjack.c	5.00 2000/11/01 xlockmore";

#endif

/* xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Wendy, let me explain something to you.  Whenever you come in here and
 * interrupt me, you're BREAKING my CONCENTRATION.  You're DISTRACTING me!
 * And it will then take me time to get back to where I was. You understand?
 * Now, we're going to make a new rule.	When you come in here and you hear
 * me typing, or whether you DON'T hear me typing, or whatever the FUCK you
 * hear me doing; when I'm in here, it means that I am working, THAT means
 * don't come in!  Now, do you think you can handle that?
 *
 * Revision History:
 * 01-Nov-2000: Allocation checks
 * 10-Oct-1998: revision history edited.
 */

#ifdef STANDALONE
#define PROGCLASS "XJack"
#define HACK_INIT init_xjack
#define HACK_DRAW draw_xjack
#define xjack_opts xlockmore_opts
#define DEFAULTS "*delay: 50000 \n" \
 "*font: \n" \
 "*text: \n" \
 "*fullrandom: True \n"
#define DEF_FONT "-*-courier-medium-r-*-*-*-240-*-*-m-*-*-*",
#define DEF_TEXT "All work and no play makes Jack a dull boy.  ";
#include "xlockmore.h"  /* in xscreensaver distribution */

#else /* STANDALONE */
#include "xlock.h"     /* in xlockmore distribution */
#endif
#include "iostuff.h"

#ifdef MODE_xjack

ModeSpecOpt xjack_opts =
{0, NULL, 0, NULL, NULL};

#ifdef USE_MODULES
ModStruct	 xjack_description =
{"xjack", "init_xjack", "draw_xjack", "release_xjack",
 "init_xjack", "init_xjack", NULL, &xjack_opts,
 50000, 1, 1, 1, 64, 1.0, "",
 "Shows Jack having one of those days", 0, NULL};

#endif

#define font_height(f) ((f==None)?8:f->ascent + f->descent)

extern XFontStruct *getFont(Display * display);

typedef struct {
	int   word_length;
	int   sentences;
	int   paras;
	Bool  caps;
	Bool  break_para;
	int   mode, stage, busyloop;
	int   left, right;      /* characters */
	int   x, y;             /* characters */
	int   hspace, vspace;   /* pixels */
	int   ascent;
	int   height;
	int   win_width, win_height;
	int   columns, rows;    /* characters */
	int   char_width, line_height;  /* pixels */
	const char *s;
	GC    gc;
} jackstruct;

static jackstruct *jacks = NULL;

static XFontStruct *mode_font = None;
static const char *source = "All work and no play makes Jack a dull boy.  ";
static const char *source_message;

extern char *message;

void
release_xjack(ModeInfo * mi)
{
	if (jacks != NULL) {
		int				 screen;

		for (screen = 0; screen < MI_NUM_SCREENS(mi); screen++) {
			jackstruct *jp = &jacks[screen];
			Display		*display = MI_DISPLAY(mi);

			if (jp->gc)
				XFreeGC(display, jp->gc);
		}
		(void) free((void *) jacks);
		jacks = NULL;
	}
	if (mode_font != None) {
		XFreeFont(MI_DISPLAY(mi), mode_font);
		mode_font = None;
	}
}

void
init_xjack(ModeInfo * mi)
{
	Display		*display = MI_DISPLAY(mi);
	jackstruct *jp;
	XGCValues gcv;


	if (jacks == NULL) {
		if ((jacks = (jackstruct *) calloc(MI_NUM_SCREENS(mi),
			 sizeof (jackstruct))) == NULL)
			return;
	}
	jp = &jacks[MI_SCREEN(mi)];
	if (mode_font == None)
		if ((mode_font = getFont(display)) == None) {
			release_xjack(mi);
		}

	if (jp->gc == NULL) {
		gcv.font = mode_font->fid;
		XSetFont(display, MI_GC(mi), mode_font->fid);
		gcv.graphics_exposures = False;
		gcv.foreground = MI_WHITE_PIXEL(mi);
		gcv.background = MI_BLACK_PIXEL(mi);
		if ((jp->gc = XCreateGC(display, MI_WINDOW(mi),
				GCForeground | GCBackground | GCGraphicsExposures | GCFont,
				&gcv)) == None) {
			return;
		}
		jp->ascent = mode_font->ascent;
		jp->height = font_height(mode_font);
	}

	jp->win_width = MI_WIDTH(mi);
	jp->win_height = MI_HEIGHT(mi);

	jp->hspace = jp->vspace = 15;  /* pixels */
	jp->char_width = (mode_font->per_char
				? mode_font->per_char['n' - mode_font->min_char_or_byte2].rbearing
				: mode_font->min_bounds.rbearing);
  if (!jp->char_width)
		jp->char_width = 1;
	jp->line_height = jp->height + 1;

	jp->columns = (jp->win_width - jp->hspace - jp->hspace) / jp->char_width;
	jp->rows = (MI_HEIGHT(mi) - jp->vspace - jp->vspace) / jp->line_height;

	jp->left = 0xFF & (NRAND((jp->columns / 2) + 1));
	if ( jp->columns - jp->left != 10 )
		jp->right = jp->left + (0xFF & (NRAND(jp->columns - jp->left - 10) + 10));
	else
		jp->right = jp->left + 10;
	jp->x = 0;
	jp->y = 0;
	jp->sentences = 0;
	jp->paras = 0;
	jp->caps = False;
	jp->break_para = True;
	jp->mode = 0;
	jp->stage = 0;
	jp->busyloop = 0;
	if (!message || !*message)
		source_message = source;
	else
		source_message = message;
	jp->s = source_message;
	MI_CLEARWINDOW(mi);
}

void
draw_xjack(ModeInfo * mi)
{
	Display		*display = MI_DISPLAY(mi);
	Window			window = MI_WINDOW(mi);
	const char *s2;
	jackstruct *jp;

	if (jacks == NULL)
		return;
	jp = &jacks[MI_SCREEN(mi)];
	if (jp->gc == None)
		return;

	jp->word_length = 0;
	for (s2 = jp->s; *s2 && *s2 != ' '; s2++)
		jp->word_length++;

	if (jp->break_para || (*(jp->s) != ' ' &&
		 (jp->x + jp->word_length) >= jp->right)) {
		jp->x = jp->left;
		jp->y++;

		if (jp->break_para)
			jp->y++;

		if (jp->mode == 1 || jp->mode == 2) {
			/* 1 = left margin goes southwest; 2 = southeast */
			jp->left += (jp->mode == 1 ? 1 : -1);
			if (jp->left >= jp->right - 10) {
				if ((jp->right < (jp->columns - 10)) && (LRAND() & 1))
					jp->right += (0xFF & (NRAND(jp->columns - jp->right)));
				else
					jp->mode = 2;
			} else if (jp->left <= 0) {
				jp->left = 0;
				jp->mode = 1;
			}
		} else if (jp->mode == 3 || jp->mode == 4) {
			/* 3 = right margin goes southeast; 4 = southwest */
			jp->right += (jp->mode == 3 ? 1 : -1);
			if (jp->right >= jp->columns) {
				jp->right = jp->columns;
				jp->mode = 4;
			} else if (jp->right <= jp->left + 10)
				jp->mode = 3;
			}

			if (jp->y >= jp->rows) {  /* bottom of page */
				/* scroll by 1-5 lines */
				int lines = ((NRAND(5)) ? 0 : (0xFF & (NRAND(5)))) + 1;

				if (jp->break_para)
					lines++;

				/* but sometimes scroll by a whole page */
				if (!NRAND(100))
					lines += jp->rows;

				while (lines > 0) {
					XCopyArea(display, window, window, jp->gc,
								0, jp->hspace + jp->line_height,
								jp->win_width,
								jp->win_height - jp->vspace - jp->vspace - jp->line_height,
								0, jp->vspace);
					XClearArea(display, window,
								0,	jp->win_height - jp->vspace - jp->line_height,
								jp->win_width,
								jp->line_height + jp->vspace + jp->vspace,
								False);
					XClearArea(display, window, 0, 0,	jp->win_width, jp->vspace, False);
					/* See? It's OK. He saw it on the television. */
					XClearArea(display, window, 0, 0, jp->hspace, jp->win_height, False);
					XClearArea(display, window,	jp->win_width - jp->vspace, 0,
								jp->hspace, jp->win_height, False);
					jp->y--;
					lines--;
#if 0
					XFlush(display);
					if (delay)
						 (void) usleep (delay * 10);
#endif
				}
				if (jp->y < 0)
					jp->y = 0;
			}

			jp->break_para = False;
		}

		if (*(jp->s) != ' ') {
			char c = *(jp->s);
			int xshift = 0, yshift = 0;

			if (!NRAND(50)) {
				xshift = NRAND((jp->char_width / 3) + 1);  /* mis-strike */
				yshift = NRAND((jp->line_height / 6) + 1);
				if (!NRAND(3))
					yshift *= 2;
				if (LRAND() & 1)
					xshift = -xshift;
				if (LRAND() & 1)
					yshift = -yshift;
			}

			if (!NRAND(250)) {  /* introduce adjascent-key typo */
				static const char * const typo[] = {
					"asqwz", "bghnv", "cdfvx", "dcefsrx", "edrsw34",
					"fcdegrtv", "gbfhtvy", "hbgjnuy", "ijkou89", "jhikmnu",
					"kijolm,", "lkop;.,", "mjkn,", "nbhjm", "oiklp09",
					"plo;[-0", "qasw12", "redft45", "sadewxz", "tfgry56",
					"uhijy78", "vbcfg", "waeqs23", "xcdsz", "yuhgt67",
					"zasx", ".l,;/",
					"ASQWZ", "BGHNV", "CDFVX", "DCEFSRX", "EDRSW#$",
					"FCDEGRTV", "GBFHTVY", "HBGJNUY", "IJKOU*(", "JHIKMNU",
					"KIJOLM,", "LKOP:><", "MJKN<", "NBHJM", "OIKLP)(",
					"PLO:{_)", "QASW!@", "REDFT$%", "SADEWXZ", "TFGRY%^",
					"UHIJY&*", "VBCFG", "WAEQS@#", "XCDSZ", "YUHGT^&",
					"ZASX", 0 };
				int i = 0;

				while (typo[i] && typo[i][0] != c)
					i++;
				if (typo[i])
					c = typo[i][0xFF & (NRAND(strlen(typo[i]+1)))];
			}

			/* caps typo */
			if (c >= 'a' && c <= 'z' && (jp->caps || !NRAND(350))) {
				c -= ('a' - 'A');
				if (c == 'O' && LRAND() & 1)
					c = '0';
			}

			{
				Bool overstrike;

				do {
					(void) XDrawString (display, window, jp->gc,
							(jp->x * jp->char_width) + jp->hspace + xshift,
							(jp->y * jp->line_height) + jp->vspace + mode_font->ascent + yshift,
							&c, 1);
					overstrike = (xshift == 0 && yshift == 0 &&
						(0 == (LRAND() & 3000)));
					if (overstrike) {
						if (LRAND() & 1)
							xshift--;
						else
							yshift--;
					}
				} while (overstrike);
			}
			if ((tolower(c) != tolower(*(jp->s)))
					? (!NRAND(10))      /* backup to correct */
					: (!NRAND(400))) {  /* fail to advance */
				jp->x--;
				jp->s--;
#if 0
				XFlush(display);
				if (delay)
					 (void) usleep (0xFFFF & (delay + (NRAND(delay * 10))));
#endif
			}
		}

		jp->x++;
		jp->s++;

		if (!NRAND(200)) {
			if (LRAND() & 1 && jp->s != source_message)
				jp->s--;   /* duplicate character */
			else if (*(jp->s))
				jp->s++;   /* skip character */
		}

		if (*(jp->s) == 0) {
			jp->sentences++;
			jp->caps = (!NRAND(40));  /* capitalize sentence */

			if (!NRAND(10) ||         /* randomly break paragraph */
					(jp->mode == 0 && (NRAND(10) || jp->sentences > 20))) {
				jp->break_para = True;
				jp->sentences = 0;
				jp->paras++;

				if (LRAND() & 1)       /* mode=0 50% of the time */
					jp->mode = 0;
				else
			 		jp->mode = (0xFF & NRAND(5));

			if (LRAND() & 1) {         /* re-pick margins */
				if (jp->columns < 3)
					jp->columns = 3;
				jp->left = 0xFF & (NRAND(jp->columns / 3));
				jp->right = jp->columns - (0xFF & (NRAND(jp->columns / 3)));

				if (!NRAND(3))        /* sometimes be wide */
					jp->right = jp->left + ((jp->right - jp->left) / 2);
			}

			if (jp->right - jp->left <= 10) {  /* introduce sanity */
				jp->left = 0;
				jp->right = jp->columns;
			}

			if (jp->right - jp->left > 50) {   /* if wide, shrink and move */
				jp->left += (0xFF & (NRAND((jp->columns - 50) + 1)));
				jp->right = jp->left + (0xFF & (NRAND(40) + 10));
			}

			/* oh, gag. */
			if (jp->mode == 0 && jp->right - jp->left < 25 && jp->columns > 40) {
				jp->right += 20;
				if (jp->right > jp->columns)
					jp->left -= (jp->right - jp->columns);
			}
		}
		jp->s = source_message;
	}

#if 0
	XFlush(display);
	if (delay) {
		(void) usleep (delay);
		if (!NRAND(3))
			(void) usleep(0xFFFFFF & ((NRAND(delay * 5)) + 1));

		if (jp->break_para)
			(void) usleep(0xFFFFFF & ((NRAND(delay * 15)) + 1));
	}
#endif

#ifdef NFS_COMPLAINTS
	if (jp->paras > 5 && (!NRAND(1000)) && jp->y < jp->rows-5) {
		int i;
		int n = NRAND(3);

		jp->y++;
		for (i = 0; i < n; i++) {
			/* See also http://catalog.com/hopkins/unix-haters/login.html */
			const char *n1 =
					"NFS server overlook not responding, still trying...";
			const char *n2 = "NFS server overlook ok.";

			while (*n1) {
				(void) XDrawString (display, window, jp->gc,
									(jp->x * jp->char_width) + jp->hspace,
									(jp->y * jp->line_height) + jp->vspace + mode_font->ascent,
									n1, 1);
				jp->x++;
				if (jp->x >= jp->columns)
					jp->x = 0, jp->y++;
				n1++;
			}
#if 0
			XFlush(display);
			(void) usleep (5000000);
#endif
			while (*n2) {
				(void) XDrawString (display, window, jp->gc,
								(jp->x * jp->char_width) + jp->hspace,
								(jp->y * jp->line_height) + jp->vspace + mode_font->ascent,
								n2, 1);
				jp->x++;
				if (jp->x >= jp->columns)
					jp->x = 0, jp->y++;
				n2++;
			}
			jp->y++;
#if 0
			XFlush(display);
			(void) usleep (500000);
#endif
		}
	}
#endif
}

#endif /* MODE_xjack */
