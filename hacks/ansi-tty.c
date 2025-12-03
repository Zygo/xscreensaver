/* xscreensaver, Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * An ANSI (VT100) terminal emulator.  Reads control codes and renders the
 * screen into a character grid.  Other programs (apple2 and phosphor) then
 * copy that layout to the screen while applying their own text styling.
 *
 *   https://en.wikipedia.org/wiki/ANSI_escape_code
 *   https://vt100.net/docs/vt100-ug/chapter3.html
 *   https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 *   https://en.wikipedia.org/wiki/ISO/IEC_2022
 *   https://invisible-island.net/vttest/
 *   https://github.com/mattiase/wraptest
 */

#include "screenhack.h"
#include "ansi-tty.h"
#include "utf8wc.h"

#define ESC  0x1B
#define CSI "\x1B["

# undef  UNDEF
# define UNDEF -0xFFFF

struct tty_state {
  char buf[255], buf2[255];
  int idx, idx2;
  int awaiting_st;
  int unicrud;
  int auto_wrap_p;
  int origin_relative_p;
  int linefeed_p;

  /* A fun quirk about ANSI / VT100 cursor addressing: When a character is
     printed in column 80, the insertion point does not wrap to the next line
     until the 81st character is printed, which will then appear in the
     leftmost column of the following line.  In this case, the cursor blinks
     atop the 80th character instead of after it. This means that a cursor at
     position 80 is visually ambiguous with one at position 79.

     Likewise, a character printed in the bottom right cell does not cause the
     screen to scroll up by one line until the *next* character is printed.

     However, this only applies to cursor coordinates caused by text insertion.
     Inserting "X" at column 80 lets cursor.x go to 81; but if at column 70
     and you do "move right by 20", the cursor ends up at 80, not 81.  Fun!

     Details: https://github.com/mattiase/wraptest
   */
  int lcf;	/* Last Column Flag */

  struct { int y1, y2; } scroll;
  struct { int x,  y, lcf;
           tty_flag flags; } saved;
  tty_flag flags;
  tty_flag g0, g1;
  tty_color fg, bg;

  char *tabs;	/* 1 in each column that has a tab stop set. */

  FILE *log_file;
};


/* The DEC Special Graphics 8-bit character set used by vt100.
   It is unclear to me if this was incorporated into ANSI.
 */
const unsigned long ansi_graphics_unicode[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 00-0F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 10-1F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 20-2F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 30-3F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 40-4F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ' ',		/* 50-5F */
  0x25C6,   /*  0x60 `    ◆  BLACK DIAMOND 				 */
  0x2592,   /*  0x61 a    ▒  MEDIUM SHADE 				 */
  0x2409,   /*  0x62 b    ␉  SYMBOL FOR HORIZONTAL TABULATION 		 */
  0x240C,   /*  0x63 c    ␌  SYMBOL FOR FORM FEED 			 */
  0x240D,   /*  0x64 d    ␍  SYMBOL FOR CARRIAGE RETURN 		 */
  0x240A,   /*  0x65 e    ␊  SYMBOL FOR LINE FEED 			 */
  0x00B0,   /*  0x66 f    °  DEGREE SIGN 				 */
  0x00B1,   /*  0x67 g    ±  PLUS-MINUS SIGN 				 */
  0x2424,   /*  0x68 h    ␤  SYMBOL FOR NEWLINE 			 */
  0x240B,   /*  0x69 i    ␋  SYMBOL FOR VERTICAL TABULATION 		 */
  0x2518,   /*  0x6A j    ┘  BOX DRAWINGS LIGHT UP AND LEFT 		 */
  0x2510,   /*  0x6B k    ┐  BOX DRAWINGS LIGHT DOWN AND LEFT 		 */
  0x250C,   /*  0x6C l    ┌  BOX DRAWINGS LIGHT DOWN AND RIGHT	 	 */
  0x2514,   /*  0x6D m    └  BOX DRAWINGS LIGHT UP AND RIGHT		 */
  0x253C,   /*  0x6E n    ┼  BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL	 */
  0x23BA,   /*  0x6F o    ⎺  HORIZONTAL SCAN LINE-1 			 */
  0x23BB,   /*  0x70 p    ⎻  HORIZONTAL SCAN LINE-3 			 */
  0x2500,   /*  0x71 q    ─  BOX DRAWINGS LIGHT HORIZONTAL 		 */
  0x23BC,   /*  0x72 r    ⎼  HORIZONTAL SCAN LINE-7 			 */
  0x23BD,   /*  0x73 s    ⎽  HORIZONTAL SCAN LINE-9 			 */
  0x251C,   /*  0x74 t    ├  BOX DRAWINGS LIGHT VERTICAL AND RIGHT	 */
  0x2524,   /*  0x75 u    ┤  BOX DRAWINGS LIGHT VERTICAL AND LEFT 	 */
  0x2534,   /*  0x76 v    ┴  BOX DRAWINGS LIGHT UP AND HORIZONTAL 	 */
  0x252C,   /*  0x77 w    ┬  BOX DRAWINGS LIGHT DOWN AND HORIZONTAL	 */
  0x2502,   /*  0x78 x    │  BOX DRAWINGS LIGHT VERTICAL 		 */
  0x2264,   /*  0x79 y    ≤  LESS-THAN OR EQUAL TO 			 */
  0x2265,   /*  0x7A z    ≥  GREATER-THAN OR EQUAL TO	 		 */
  0x03C0,   /*  0x7B {    π  GREEK SMALL LETTER PI 			 */
  0x2260,   /*  0x7C |    ≠  NOT EQUAL TO 				 */
  0x00A3,   /*  0x7D }    £  POUND SIGN 				 */
  0x00B7,   /*  0x7E ~    ·  MIDDLE DOT 				 */
  0,        /*  0x7F							 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 80-8F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 90-9F */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* A0-AF */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* B0-BF */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* C0-CF */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* D0-DF */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* E0-EF */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0		/* F0-FF */
};


static void
set_color (ansi_tty *tty, int fg_p, int color)
{
  tty_state *st = tty->state;
  tty_color *c = (fg_p ? &st->fg : &st->bg);
  if (color < countof(tty->cmap))
    {
      *c = tty->cmap[color][fg_p ? 0 : 1];
    }
  else
    {
      c->r = (color >> 16) & 0xFF;
      c->g = (color >> 8)  & 0xFF;
      c->b = (color >> 0)  & 0xFF;
    }
}


static void
ansi_tty_default_tabs (ansi_tty *tty)
{
  int ts = 8;
  int i;
  char *tabs = tty->state->tabs;
  for (i = 0; i < tty->width; i++)
    tabs[i] = (i % ts) ? 0 : 1;
}


static void
ansi_tty_reset (ansi_tty *tty)
{
  int odebug = tty->debug_p;
  FILE *olog = tty->state->log_file;
  char *otabs = tty->state->tabs;
  memset (tty->grid, 0, tty->width * tty->height * sizeof (*tty->grid));
  memset (tty->state, 0, sizeof(*tty->state));
  tty->state->scroll.y1 = 0;
  tty->state->scroll.y2 = tty->height;
  tty->state->auto_wrap_p = True;
  tty->state->fg = tty->cmap[2][0];	/* Green */
  tty->state->bg = tty->cmap[0][1];	/* Black */
  tty->debug_p = odebug;
  tty->state->log_file = olog;
  tty->state->tabs = otabs;
  ansi_tty_default_tabs (tty);
}


ansi_tty *
ansi_tty_init (int w, int h)
{
  ansi_tty *tty = (ansi_tty *) calloc (1, sizeof(*tty));
  if (!tty) abort();
  if (w <= 2 || h <= 2) abort();
  tty->width  = w;
  tty->height = h;
  tty->grid   = (tty_char *) calloc (w * h, sizeof (*tty->grid));
  tty->state  = (tty_state *) calloc (1, sizeof(*tty->state));
  if (!tty->grid || !tty->state) abort();
  tty->state->tabs = (char *) calloc (w, 1);
  ansi_tty_default_tabs (tty);

  tty->debug_p = 1;

# if !defined(HAVE_MOBILE) && !defined(__OPTIMIZE__)
  if (tty->debug_p >= 4)
    {
      const char *fn = "/tmp/ttylog.txt";
      tty->state->log_file = fopen (fn, "wb");
      if (!tty->state->log_file)
        fprintf (stderr, "%s: unable to write %s\n", progname, fn);
      else
        fprintf (stderr, "%s: WARNING: logging to %s\n", progname, fn);

      /* Note: if you're trying to do playback using this log file,
         be sure to do "stty -onlcr ; cat FILE" or the shell will
         turn \r\n into \r\r\n. */
    }
# endif

  {
    static const tty_color cmap[] = {		/*        VGA colors     */
      { 0x00, 0x00, 0x00 },			/* 30 40  Black          */
      { 0xAA, 0x00, 0x00 },			/* 31 41  Red            */
      { 0x00, 0xAa, 0x00 },			/* 32 42  Green          */
      { 0xAA, 0x55, 0x00 },			/* 33 43  Yellow         */
      { 0x00, 0x00, 0xAA },			/* 34 44  Blue           */
      { 0xAA, 0x00, 0xAA },			/* 35 45  Magenta        */
      { 0x00, 0xAA, 0xAA },			/* 36 46  Cyan           */
      { 0xAA, 0xAA, 0xAA },			/* 37 47  White          */
      { 0x55, 0x55, 0x55 },			/* 90 100 Bright Black   */
      { 0xFF, 0x55, 0x55 },			/* 91 101 Bright Red     */
      { 0x55, 0xFF, 0x55 },			/* 92 102 Bright Green   */
      { 0xFF, 0xFF, 0x55 },			/* 93 103 Bright Yellow  */
      { 0x55, 0x55, 0xFF },			/* 94 104 Bright Blue    */
      { 0xFF, 0x55, 0xFF },			/* 95 105 Bright Magenta */
      { 0x55, 0xFF, 0xFF },			/* 96 106 Bright Cyan    */
      { 0xFF, 0xFF, 0xFF },			/* 97 107 Bright White   */
    };
    int i;
    for (i = 0; i < countof(cmap); i++)
      {
        tty->cmap[i][0] = cmap[i];
        tty->cmap[i][1] = cmap[i];
      }
  }

  ansi_tty_reset (tty);

  if (tty->debug_p > 1)
    fprintf (stderr, "%s: tty init %dx%d\n", progname, w, h);

  return tty;
}

void
ansi_tty_free (ansi_tty *tty)
{
  if (tty->state->log_file)
    fclose (tty->state->log_file);
  free (tty->state->tabs);
  free (tty->state);
  free (tty);
}

void ansi_tty_resize (ansi_tty *tty, int w, int h)
{
  tty_state *st = tty->state;
  tty_char *grid2;
  int x, y;
  if (w <= 2 || h <= 2) abort();
  grid2 = (tty_char *) calloc (w * h, sizeof (*grid2));

  for (y = 0; y < tty->height; y++)
    for (x = 0; x < tty->width; x++)
      if (x < w && y < h)
        grid2[y * w + x] = tty->grid[y * tty->width + x];

  free (tty->grid);
  tty->grid   = grid2;
  tty->width  = w;
  tty->height = h;

  tty->state->tabs = (char *) realloc (tty->state->tabs, w);

  if (tty->x >= w) tty->x = w-1;
  if (st->scroll.y1 > tty->height) st->scroll.y1 = tty->height;
  if (st->scroll.y2 > tty->height) st->scroll.y2 = tty->height;
  if (tty->y >= st->scroll.y2) tty->y = st->scroll.y2 - 1;
  if (tty->y >= st->scroll.y2) tty->y = st->scroll.y2 - 1;

  if (tty->debug_p)
    fprintf (stderr, "%s: tty resize %dx%d\n", progname, w, h);
}


/* Scroll lines up or down within the character grid.
 */
static void
tty_scroll (ansi_tty *tty, int lines)
{
  tty_state *st = tty->state;
  unsigned char *top = (unsigned char *)
                       (tty->grid + tty->width * st->scroll.y1);
  unsigned char *bot = (unsigned char *)
                       (tty->grid + (tty->width * st->scroll.y2));
  int total = bot - top;
  int move  = lines * tty->width * sizeof(*tty->grid);

  if ( lines >= st->scroll.y2 - st->scroll.y1 ||
      -lines >= st->scroll.y2 - st->scroll.y1)
    {
      /* Scrolling more lines than exist is clearing. */
      memset (top, 0, total);
    }
  else if (lines > 0)
    {
      memmove (top, top + move, total - move);
      memset (top + total - move, 0, move);
    }
  else if (lines < 0)
    {
      move = -move;
      memmove (top + move, top, total - move);
      memset (top, 0, move);
    }
}


/* Clear characters from the grid.
   Start and end positions are inclusive.
 */
static void
tty_erase (ansi_tty *tty, int x1, int y1, int x2, int y2)
{
  int from, to;
  tty_char *ch, *end;

  if (x1 < 0) x1 = 0;
  if (x2 < 0) x2 = 0;
  if (y1 < 0) y1 = 0;
  if (y2 < 0) y2 = 0;
  if (x1 > tty->width-1)  x1 = tty->width-1;
  if (x2 > tty->width-1)  x2 = tty->width-1;
  if (y1 > tty->height-1) y1 = tty->height-1;
  if (y2 > tty->height-1) y2 = tty->height-1;

  from = y1 * tty->width + x1;
  to   = y2 * tty->width + x2;
  if (from > to) abort();

  ch  = tty->grid + from;
  end = tty->grid + to;

  while (ch <= end)
    {
# if 1
      ch->c     = 0;
      ch->flags = 0;
      ch->fg    = tty->cmap[2][0];	/* Green */
      ch->bg    = tty->cmap[0][1];	/* Black */
# else
      /* You might assume that turning on inverse and clearing to end of
         line would give you an inverted line, but you would be wrong. */
      ch->c     = tty->state->flags ? ' ' : 0;
      ch->flags = tty->state->flags;
      ch->fg    = tty->state->fg;
      ch->bg    = tty->state->bg;
# endif
      ch++;
    }
}


/* Print a line describing the non-command text that was printed,
   once enough of it has buffered up to be legible. NULL means flush.
 */
static void
tty_log_c (ansi_tty *tty, const char c)
{
  if (tty->debug_p > 2)
    {
      tty_state *st = tty->state;
      char buf3[sizeof (st->buf2) * 4 + 100];
      char *in, *out;

      if (c)
        {
          st->buf2[st->idx2++] = c;
          st->buf2[st->idx2] = 0;
        }

      if (((c >= ' ' && c <= '~') || c == '\r' || c == 0x08) &&
          st->idx2 < sizeof(st->buf2) - 1)
        return;

      in = st->buf2;
      out = buf3;
      *out = 0;

      while (*in) {
        char c = *in++;
        if (c >= ' ' && c <= '~')
          {
            *out++ = c;
            *out = 0;
          }
        else
          {
            int pad = False;
            if      (c == 0x08) strcat (out, "\\b");
            else if (c == 0x09) strcat (out, "\\t");
            else if (c == 0x0A) strcat (out, "\\n");
            else if (c == 0x1B) strcat (out, "\\e");
            else if (c == 0x0D) strcat (out, "\\r");
            else
              {
                pad = True;
                if (out > buf3 && out[-1] != ' ')
                  {
                    *out++ = ' ';
                    *out = 0;
                  }
                sprintf (out, "\\x%02X", (unsigned char) c);
              }

            out += strlen (out);
            if (*in && pad)
              {
                *out++ = ' ';
                *out = 0;
              }
          }
      }

      if (out > buf3)
        fprintf (stderr, "%s: txt: \"%s\"\n", progname, buf3);

      st->idx2 = 0;
      st->buf2[0] = 0;
    }
}


/* Print a line describing the terminal command being executed.
 */
static void
tty_log (ansi_tty *tty, const char *kind, int ok, const char *log,
         int ac, const int *av, const char *cmd)
{
  if (tty->debug_p > (ok ? 1 : 0))
    {
      char buf[1024], *s = buf;
      int i;

      tty_log_c (tty, 0);  /* Flush */

      if (ac) *s++ = ':';
      *s = 0;

      for (i = 0; i < ac; i++)
        {
          if (av[i] == UNDEF)
            sprintf (s, " -");
          else
            sprintf (s, " %d", av[i]);
          s += strlen(s);
        }
      if (!ok || tty->debug_p > 1)
        {
          int j = 0;
          strcat (s, ": ");
          s += strlen(s);
          while (*cmd && j < 100)
            {
              if (*cmd == ESC)
                strcat (s, "ESC ");
              else if (*cmd < ' ' || *cmd > '~')
                sprintf (s, "0x%02X", *cmd);
              else
                sprintf (s, "%c", *cmd);

              s += strlen(s);
              cmd++;
              j++;
            }
        }

      fprintf (stderr, "%s: %3s: %s%s%s\n", progname, kind,
               (ok ? "" : "Unimplemented: "), log, buf);
    }
}


/* Just gonna leave this here:

	% infocmp -x vt100
	Reconstructed via infocmp from file: /usr/share/terminfo/v/vt100
	vt100|vt100-am|DEC VT100 (w/advanced video),
	OTbs, am, mc5i, msgr, xenl, xon,
	cols#80, it#8, lines#24, vt#3,
	acsc=``aaffggjjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~,
	bel=^G, blink=\E[5m$<2>, bold=\E[1m$<2>,
	clear=\E[H\E[J$<50>, cr=\r, csr=\E[%i%p1%d;%p2%dr,
	cub=\E[%p1%dD, cub1=^H, cud=\E[%p1%dB, cud1=\n,
	cuf=\E[%p1%dC, cuf1=\E[C$<2>,
	cup=\E[%i%p1%d;%p2%dH$<5>, cuu=\E[%p1%dA,
	cuu1=\E[A$<2>, ed=\E[J$<50>, el=\E[K$<3>, el1=\E[1K$<3>,
	enacs=\E(B\E)0, home=\E[H, ht=^I, hts=\EH, ind=\n, ka1=\EOq,
	ka3=\EOs, kb2=\EOr, kbs=^H, kc1=\EOp, kc3=\EOn, kcub1=\EOD,
	kcud1=\EOB, kcuf1=\EOC, kcuu1=\EOA, kent=\EOM, kf0=\EOy,
	kf1=\EOP, kf10=\EOx, kf2=\EOQ, kf3=\EOR, kf4=\EOS, kf5=\EOt,
	kf6=\EOu, kf7=\EOv, kf8=\EOl, kf9=\EOw, lf1=pf1, lf2=pf2,
	lf3=pf3, lf4=pf4, mc0=\E[0i, mc4=\E[4i, mc5=\E[5i, rc=\E8,
	rev=\E[7m$<2>, ri=\EM$<5>, rmacs=^O, rmam=\E[?7l,
	rmkx=\E[?1l\E>, rmso=\E[m$<2>, rmul=\E[m$<2>,
	rs2=\E<\E>\E[?3;4;5l\E[?7;8h\E[r, sc=\E7,
	sgr=\E[0%?%p1%p6%|%t;1%;%?%p2%t;4%;%?%p1%p3%|%t;7%;%?%p4%t;5%;m%?%p9%t\016%e\017%;$<2>,
	sgr0=\E[m\017$<2>, smacs=^N, smam=\E[?7h, smkx=\E[?1h\E=,
	smso=\E[7m$<2>, smul=\E[4m$<2>, tbc=\E[3g,
	u6=\E[%i%d;%dR, u7=\E[6n, u8=\E[?%[;0123456789]c, u9=\EZ,

   Reformatted for legibility:

	OTbs
	auto_right_margin
	prtr_silent
	move_standout_mode
	eat_newline_glitch
	xon_xoff
	cols#80
	init_tabs#8
	lines#24
	virtual_terminal#3

	acs_chars	= ``aaffggjjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~
	bell			= ^G
	enter_blink_mode	= CSI 5 m
	enter_bold_mode		= CSI 1 m
	clear_screen		= CSI H
				  CSI J
	carriage_return		= \r
	change_scroll_region	= CSI <FROM> ; <TO> r
	parm_left_cursor	= CSI <N> D
	cursor_left		= ^H
	parm_down_cursor	= CSI <N> B
	cursor_down		= \n
	parm_right_cursor	= CSI <N> C
	cursor_right		= CSI C
	cursor_address		= CSI <X> ; <Y> H
	parm_up_cursor		= CSI <N> A
	cursor_up		= CSI A
	clr_eos			= CSI J
	clr_eol			= CSI K
	clr_bol			= CSI 1 K
	ena_acs			= ESC ( B ESC ) 0
	cursor_home		= CSI H
	tab			= ^I
	set_tab			= ESC H
	scroll_forward		= \n
	key_a1			= ESC O q
	key_a3			= ESC O s
	key_b2			= ESC O r
	key_backspace		= ^H
	key_c1			= ESC O p
	key_c3			= ESC O n
	key_left		= ESC O D
	key_down		= ESC O B
	key_right		= ESC O C
	key_up			= ESC O A
	key_enter		= ESC O M
	key_f0			= ESC O y
	key_f1			= ESC O P
	key_f10			= ESC O x
	key_f2			= ESC O Q
	key_f3			= ESC O R
	key_f4			= ESC O S
	key_f5			= ESC O t
	key_f6			= ESC O u
	key_f7			= ESC O v
	key_f8			= ESC O l
	key_f9			= ESC O w
	lab_f1			= pf1
	lab_f2			= pf2
	lab_f3			= pf3
	lab_f4			= pf4
	print_screen		= CSI 0 i
	prtr_off		= CSI 4 i
	prtr_on			= CSI 5 i
	restore_cursor		= ESC 8
	enter_reverse_mode	= CSI 7 m
	scroll_reverse		= ESC M
	exit_alt_charset_mode	= ^O
	exit_am_mode		= CSI ? 7 l
	keypad_local		= CSI ? 1 l ESC >
	exit_standout_mode	= CSI m
	exit_underline_mode	= CSI m
	reset_2string		= ESC < ESC >
				  CSI ? 3 ; 4 ; 5 l
				  CSI ? 7 ; 8 h
				  CSI r
	save_cursor		= ESC 7

	set_attributes		= CSI 0
				  IF  %p1 | %p6  THEN  ; 1  ENDIF
				  IF  %p2        THEN  ; 4  ENDIF
				  IF  %p1 | %p3  THEN  ; 7  ENDIF
				  IF  %p4        THEN  ; 5  ENDIF
				  m
				  IF  %p9        THEN  \016
				  ELSE  \017		    ENDIF

	exit_attribute_mode	= CSI m \017
	enter_alt_charset_mode	= ^N
	enter_am_mode		= CSI ? 7 h
	keypad_xmit		= CSI ? 1 h ESC =
	enter_standout_mode	= CSI 7 m
	enter_underline_mode	= CSI 4 m
	clear_all_tabs		= CSI 3 g
	user6			= CSI %i %d ; %d R
	user7			= CSI 6 n
	user8			= CSI ? %[;0123456789] c
	user9			= ESC Z
 */


/* I find state machines soothing. If the thing you're writing is not
   a state machine, it might be evil.
     _______________________________________________________________
    /                                                               \
    |  Tech enthusiasts: My entire house is smart.                  |
    |                                                               |
    |  Tech workers: The only piece of technology in my house is a  |
    |  printer and I keep a gun next to it so I can shoot it if it  |
    |  makes a noise I don't recognize.          -- PPathole, 2019  |
    \                                                               |
    `---------------------------------------------------------------'
 */
void
ansi_tty_print (ansi_tty *tty, unsigned long c)
{
  tty_state *st = tty->state;
  int done = False;
  int scrolled_p = False;

  const char *kind = "?";
  int av[255] = { UNDEF };
  int ac = 0;
  int i;

  if (st->log_file)
    {
      fputc ((char) c, st->log_file);  /* Botches unicode */
      fflush (st->log_file);
    }

# define LOG(S) tty_log (tty, kind, True,  (S), ac, av, st->buf)
# define UND(S) tty_log (tty, kind, False, (S), ac, av, st->buf)


  /* Parse the command sequences.

     nF 	20-2F	 SPC - /   	2+ bytes, [20-2F]+ 30-7E
     Fp 	30-3F	 0-9:;<=>?	2 bytes
     Fe 	40-5F	 @A-Zp\]^_	2 bytes, or CSI: ESC [ ... C
     Fs 	60-7E	 `a-z{|}~	2 bytes
   */

  /* WTF, BS and others are allowed within command sequences! */
  if (st->idx > 0 && c < ' ')
    goto SELF_INSERT_INTERLUDE;

  st->buf[st->idx++] = c;
  st->buf[st->idx]   = 0;


  /******************************************************************** ESC */


  if (st->idx >= sizeof(st->buf) - 2)			 /* Buffer overflow */
    {
      kind = "?";
      done = True;
      UND ("OVERFLOW");
    }
  else if (st->awaiting_st)
    {
      kind = "ST0";
      /* Some "Fe" commands swallow all bytes until an "ST" string terminator
         command is received, so just keep going, filling the buffer. */
    }
  else if (c == ESC)				    /* Starting any command */
    {
      kind = "ESC";
      if (st->idx > 1)
        LOG ("Aborted by ESC");
      st->idx = 1;
    }


  /********************************************************************* nF */


  else if (st->idx == 2 &&			/* nF escape sequence start */
           st->buf[0] == ESC &&
           c >= 0x20 &&				/*    !"#$%&'()*+,-./       */
           c <= 0x2F)
    {
      kind = "nFa";
    }
  else if (st->idx > 2 &&			/* nF escape sequence cont. */
           st->buf[0] == ESC  &&
           st->buf[1] >= 0x20 &&		/*    !"#$%&'()*+,-./       */
           st->buf[1] <= 0x2F &&
           c >= 0x20 &&				/*    !"#$%&'()*+,-./       */
           c <= 0x2F)
    {
      kind = "nFb";
    }
  else if (st->idx > 2 &&			/* nF escape sequence end   */
           st->buf[0] == ESC  &&
           st->buf[1] >= 0x20 &&		/*    !"#$%&'()*+,-./       */
           st->buf[1] <= 0x2F &&
           c >= 0x30 &&				/*    0 - ~                 */
           c <= 0x7E)
    {
      kind = "nF";
      done = True;

      switch (st->idx) {
      case 2:					/* ESC ? */
        kind = "nF2";
        switch (st->buf[1]) {
        case '`':	UND ("Disable manual input");			break;
        case 'a':	UND ("Interrupt");				break;
        case 'b':	UND ("Enable manual input");			break;
        case 'c':	LOG ("Reset");
          ansi_tty_reset (tty);
          break;
        case 'd':	UND ("Coding method delimiter");		break;
        case 'n':	UND ("Locking shift 1");			break;
        case 'o':	UND ("Locking shift 3");			break;
        case '|':	UND ("Locking shift 3R");			break;
        case '}':	UND ("Locking shift 2R");			break;
        case '~':	UND ("Locking shift 1R");			break;
        default:	UND ("Unknown");				break;
        }
        break;

      case 3:					/* ESC ? ? */
        kind = "nF3";
        switch (st->buf[1]) {
        case '!':	UND ("C0-designate");   			break;
        case '"':	UND ("C1-designate");   			break;

        case '#':				/* ESC # ? */
          st->lcf = False;
          switch (st->buf[2]) {
          case '3':	UND ("Double Height Top");			break;
          case '4':	UND ("Double Height Bottom");			break;
          case '5':	UND ("Single Width");				break;
          case '6':	UND ("Double Width");				break;
          case '8':	LOG ("Screen Alignment");
            {
              tty_char  *ch = tty->grid;
              tty_char *end = tty->grid + (tty->width * tty->height);
              while (ch < end)
                {
                  ch->c = 'E';
                  ch++;
                }
            }
            break;
          default:	UND ("Unknown #");				break;
          }
          break;

        case '$':	UND ("G0 designate");   			break;

        case '%':				/* ESC % ? */
          switch (st->buf[2]) {
          case '@':	UND ("Designate coding");			break;
          case 'B':	UND ("UTF-1");					break;
          case 'G':	UND ("UTF-8");					break;
          case '/':	UND ("UTF-32");					break;
          default:	UND ("Unknown %");				break;
          }
          break;
        case '&':	UND ("Identify revised registration");		break;
        case '\'':	UND ("Not used");				break;

        case '(':				/* ESC ( ? */
        case ')':				/* ESC ( ? */
          {
            int g0_p = (st->buf[1] == '(');
            tty_flag *gg = (g0_p ? &st->g0 : &st->g1);

            /* This says which character set g0 and g1 indicate. */
            *gg = 0;
            switch (st->buf[2]) {
            case '0':	LOG ("G0-designate 94-set: Line Drawing");
              *gg = TTY_SYMBOLS;
              break;

              /* We could map these various fonts, particularly the
                 line- and box-drawing font, to their corresponding
                 Unicode code points.  But since neither Phosphor and
                 Apple2 can display non-Latin1 characters, that
                 wouldn't do us any good right now. */

            case '1':	UND ("Gx-designate 94: Alt Char");		break;
            case '2':	UND ("Gx-designate 94: Alt Special");		break;
            case '5':	UND ("Gx-designate 94: Finnish");		break;
            case '7':	UND ("Gx-designate 94: Swedish");		break;
            case '9':	UND ("Gx-designate 94: French Canadian");	break;
            case '<':	UND ("Gx-designate 94: DEC Supplemental");	break;
            case '>':	UND ("Gx-designate 94: DEC Technical");		break;
            case 'A':	UND ("Gx-designate 94: UK");			break;
            case 'B':	LOG ("Gx-designate 94: ASCII");			break;
            case 'C':	UND ("Gx-designate 94: Finnish");		break;
            case 'H':	UND ("Gx-designate 94: Swedish");		break;
            case 'I':	UND ("Gx-designate 94: JIS Katakana");		break;
            case 'J':	UND ("Gx-designate 94: JIS Roman");		break;
            case 'K':	UND ("Gx-designate 94: German");		break;
            case 'Q':	UND ("Gx-designate 94: French Canadian");	break;
            case 'R':	UND ("Gx-designate 94: French");		break;
            case 'Y':	UND ("Gx-designate 94: Italian");		break;
            case 'Z':	UND ("Gx-designate 94: Spanish");		break;
            case 'f':	UND ("Gx-designate 94: French");		break;
            default:	UND ("Unknown Gx");				break;
            }
          }
          break;
						/* ESC # # */

        case '*':	UND ("G2-designate 94");			break;
        case '+':	UND ("G3-designate 94");			break;
        case ',':	UND ("Not used");				break;
        case '-':	UND ("G1-designate 96");			break;
        case '.':	UND ("G2-designate 96");			break;
        case '/':	UND ("G3-designate 96");			break;

        case ' ':				/* ESC SP # */
          switch (st->buf[2]) {
          case 'A':	UND ("G0 in GL, GR unused");			break;
          case 'B':	UND ("G0 and G1 to GL");			break;
          case 'C':	UND ("G0 in GL, G1 in GR, no lock");		break;
          case 'D':	UND ("G0 in GL, G1 in GR 8-bit");		break;
          case 'E':	UND ("Shift preserved");			break;
          case '"':	UND ("C1 controls esc");			break;
          case 'G':	UND ("C1 controls CR");				break;
          case 'H':	UND ("94-char graphical");			break;
          case 'I':	UND ("94-char and/or 96-char");			break;
          case 'J':	UND ("7-bit");					break;
          case 'K':	UND ("8-bit");					break;
          case 'L':	UND ("ISO/IEC 4873 1");				break;
          case 'M':	UND ("ISO/IEC 4873 2");				break;
          case 'N':	UND ("ISO/IEC 4873 3");				break;
          case 'P':	UND ("SI / LS0");				break;
          case 'R':	UND ("SO / LS1");				break;
          case 'S':	UND ("LS1R 8-bit");				break;
          case 'T':	UND ("LS2");					break;
          case 'U':	UND ("LS2R 8-bit");				break;
          case 'V':	UND ("LS3");					break;
          case 'W':	UND ("LS3R 8-bit");				break;
          case 'Z':	UND ("SS2");					break;
          case '%':	UND ("Designate other");			break;
          case '[':	UND ("SS2");					break;
          case '\\':	UND ("Single-shift GR");			break;
          default:	UND ("Unknown SP");				break;
          }
          break;
        default:	UND ("Unknown");				break;
        }
        break;

      case 4:			/* ESC # # # */
        kind = "nF4";
        switch (st->buf[1]) {
        case '$':		/* ESC $ # # */
          switch (st->buf[2]) {
          case '(':	UND ("G0 designate");				break;
          case ')':	UND ("G1-designate");				break;
          case '*':	UND ("G2-designate");				break;
          case '+':	UND ("G3-designate multi 94");			break;
          case ',':	UND ("Not used");				break;
          case '-':	UND ("G1-designate multi 96");			break;
          case '.':	UND ("G2-designate multi 96");			break;
          case '/':	UND ("G3-designate multi 96");			break;
          default:	UND ("Unknown $");				break;
          }
        case '%':		/* ESC % # # */
          switch (st->buf[2]) {
          case '/':
            switch (st->buf[3]) {
            case 'I':	UND ("UTF-8");					break;
            case 'L':	UND ("UTF-16");					break;
            default:	UND ("Unknown %");				break;
            }
            break;
          default:	UND ("Unknown");				break;
          }
          break;
        default:	UND ("Unknown");				break;
        }
        break;
      default:		UND ("Unknown");				break;
      }
    }


  /********************************************************************* Fp */


  else if (st->idx == 2 &&			      /* Fp escape sequence */
           st->buf[0] == ESC &&
           c >= 0x30 &&				      /*    0-9:;<=>?       */
           c <= 0x3F)
    {
      kind = "Fp";
      done = True;

      switch (c) {
      case '7':				LOG ("Save Cursor");
        st->saved.flags = st->flags;
        st->saved.lcf = st->lcf;
        st->saved.x = tty->x;
        st->saved.y = tty->y;
        break;
      case '8':				LOG ("Restore Cursor");
        st->flags = st->saved.flags;
        st->lcf = st->saved.lcf;
        tty->x = st->saved.x;
        tty->y = st->saved.y;
        break;
      case '9':				UND ("Forward Index");		break;
      case '=':			     /* UND ("App Keypad");*/		break;
      case '<':				UND ("ANSI mode");		break;
      case '>':			     /* UND ("Normal Keypad");*/	break;
      default:				UND ("Unknown");		break;
      }
    }
  else if (st->idx >= 3 &&			/* CSI: Sequence Introducer */
           st->buf[0] == ESC &&			/*      intermediate bytes  */
           st->buf[1] == '[' &&			/*      ESC [ ... C         */
           ((c >= 0x30 && c <= 0x3F) ||		/* parameters    0–9:;<=>?  */
            (c >= 0x20 && c <= 0x2F)))		/* others  !"#$%&'()*+,-./  */
    {
      kind = "CSIa";
    }


  /******************************************************************** CSI */


  else if (st->idx >= 3 &&			/* CSI: Sequence Introducer */
           st->buf[0] == ESC &&			/*      final byte          */
           st->buf[1] == '[' &&			/*      ESC [ ... C         */
           c >= 0x40 &&				/* cmd  @A–Z[\]^_`a–z{|}~   */
           c <= 0x7E)
    {
      kind = "CSI";
      done = True;

      /* Parse the "80;24;365" args into 'av'. */
      {
        int any = False;
        for (i = 0; i < countof(av); i++)
          av[i] = UNDEF;
        for (i = 2; i < st->idx - 1; i++)
          {
            if (st->buf[i] == ';' && ac < countof(av)-1)
              {
                if (ac >= countof(av) - 2)
                  break;
                ac++;
                av[ac] = 0;
                any = True;
              }
            else if (st->buf[i] >= '0' && st->buf[i] <= '9')
              {
                if (av[ac] == UNDEF)
                  av[ac] = 0;
                av[ac] = (av[ac] * 10) + (st->buf[i] - '0');
                any = True;
              }
            else if (st->buf[i] >= 0x3C && st->buf[i] <= 0x3F)
              ;  /* Characters "<=>?" indicate private commands. */
            else if (st->buf[i] == '!')
              ;  /* What's this? */
            else
              UND ("Weird parameters");
          }
        if (any) ac++;
      }

      switch (c) {
      case '@':	st->lcf = False;		UND ("Shift left");	break;
      case 'A':					LOG ("Cursor Up");
        /* "CSI n SP A" means shift right n cols */
        if (av[0] == UNDEF || av[0] == 0) av[0] = 1;
        tty->y -= av[0];
        st->lcf = False;
        scrolled_p = True;
        break;
      case 'B':					LOG ("Cursor Down");
        if (av[0] == UNDEF || av[0] == 0) av[0] = 1;
        tty->y += av[0];
        st->lcf = False;
        scrolled_p = True;
        break;
      case 'C':					LOG ("Cursor Forward");
        if (av[0] == UNDEF || av[0] == 0) av[0] = 1;
        tty->x += av[0];
        st->lcf = False;
        scrolled_p = True;
        break;
      case 'D':					LOG ("Cursor Back");
        if (av[0] == UNDEF || av[0] == 0) av[0] = 1;
        tty->x -= av[0];
        st->lcf = False;
        scrolled_p = True;
        break;
      case 'E':					LOG ("Cursor Next Line");
        if (av[0] == UNDEF) av[0] = 1;
        tty->x = 0;
        tty->y += av[0];
        scrolled_p = True;
        /* Set lcf to False here? */
        break;
      case 'F':					LOG ("Cursor Previous Line");
        if (av[0] == UNDEF) av[0] = 1;
        tty->x = 0;
        tty->y -= av[0];
        scrolled_p = True;
        /* Set lcf to False here? */
        break;
      case 'G':					LOG ("Cursor Horizontal Abs");
        if (av[0] == UNDEF || av[0] == 0) av[0] = 1;
        tty->x = av[0]-1;
        /* Set lcf to False here? */
        break;
      case 'H':					LOG ("Cursor Position");
        if (av[0] == UNDEF || av[0] == 0) av[0] = 1;
        if (av[1] == UNDEF || av[1] == 0) av[1] = 1;
        tty->y = av[0]-1;
        tty->x = av[1]-1;
        if (st->origin_relative_p) tty->y += st->scroll.y1;
        st->lcf = False;
        break;

      case 'I':					LOG ("Fwd Tab Stop");
        {
          int i;
          if (av[0] == UNDEF || av[0] == 0) av[0] = 1;
          /* Move forward N tab stops. */
          /* Set lcf to False here? */
          for (i = 0; i < av[0]; i++)
            {
              tty->x++;
              while (tty->x < tty->width &&
                     !st->tabs[tty->x])
                tty->x++;
            }
        }
	break;

      case 'J':				       /* Erase in Display */
        st->lcf = False;
        if (av[0] == UNDEF || av[0] == 0)
          {					LOG ("Erase to end of screen");
            tty_erase (tty,
                       tty->x, tty->y,
                       tty->width-1, tty->height-1);
          }
        else if (av[0] == 1)
          {				      LOG ("Erase to start of screen");
            tty_erase (tty,
                       0, 0,
                       tty->x-1, tty->y);
            }
        else if (av[0] == 2 || av[0] == 3)
          {					LOG ("Erase screen");
            tty_erase (tty,
                       0, 0,
                       tty->width-1, tty->height-1);
          }
        /* CSI ? n J" are vt220 */
        break;

      case 'K':					/* Erase in Line */
        st->lcf = False;
        if (av[0] == UNDEF || av[0] == 0)
          {					LOG ("Erase to end of line");
            tty_erase (tty,
                       tty->x, tty->y,
                       tty->width-1, tty->y);
          }
        else if (av[0] == 1)
            {					LOG ("Erase to start of line");
              tty_erase (tty,
                         0, tty->y,
                         tty->x-1, tty->y);
            }
          else if (av[0] == 2 || av[0] == 3)
            {					LOG ("Erase line");
              tty_erase (tty,
                         0, tty->y,
                         tty->width-1, tty->y);
            }
        break;

      case 'L':					UND ("Insert lines");	break;
      case 'M':					UND ("Delete lines");	break;
      case 'P':	st->lcf = False;		UND ("Delete chars");	break;
      case 'Q':					UND ("Palette stack");	break;
      case 'R':					UND ("Palette restore");break;

      case 'S':					LOG ("Scroll Up");
        if (av[0] == UNDEF) av[0] = 1;
        tty_scroll (tty, av[0]);
        scrolled_p = True;
        /* Set lcf to False here? */
        break;
      case 'T':					LOG ("Scroll Down");
        if (av[0] == UNDEF) av[0] = 1;
        tty_scroll (tty, -av[0]);
        scrolled_p = True;
        /* Set lcf to False here? */
        break;

      case 'W':					LOG ("Reset tabs");
        ansi_tty_default_tabs (tty);
        break;

      case 'X': st->lcf = False;		UND ("Erase chars");	break;
      case 'Z':					LOG ("Back tab stop");
        {
          int i;
          if (av[0] == UNDEF || av[0] == 0) av[0] = 1;
          /* Move backward N tab stops. */
          for (i = 0; i < av[0]; i++)
            {
              tty->x--;
              while (tty->x >= 0 &&
                     !st->tabs[tty->x])
                tty->x--;
            }
        }
        break;

      /* Unused: [\]_ */

      case '^':					UND ("Scroll down");	break;
      case '`':					UND ("Position abs");	break;

      case 'a':					UND ("Position rel");	break;
      case 'b':					UND ("Repeat prev");	break;

      case 'c':							 /* Reports */
        switch (av[0]) {
        case 0:			LOG ("Report model");
          if (tty->tty_send)
            {
              char buf[40];	/* Base model */
              sprintf (buf, CSI "?%d;%dc", 1, 0);
              (*tty->tty_send) (tty->closure, buf);
            }
          break;
        default:		UND ("Unknown report request");		break;
        break;
        }
        break;

      case 'd':					UND ("Pos abs");	break;
      case 'e':					UND ("Pos rel");	break;

      case 'f':					LOG ("H/V Position");
        if (av[0] == UNDEF || av[0] == 0) av[0] = 1;
        if (av[1] == UNDEF || av[1] == 0) av[1] = 1;
        tty->y = av[0]-1;
        tty->x = av[1]-1;
        st->lcf = False;
        break;

      case 'g':
        if (av[0] == UNDEF) av[0] = 0;
        switch (av[0]) {
        case 0:					LOG ("Clear tab");
          st->tabs[tty->x] = 0;
          break;
        case 1:					LOG ("Set tab");
          st->tabs[tty->x] = 1;
          break;
        case 3:					LOG ("Clear all tabs");
          memset (st->tabs, 0, tty->width);
          break;
        default: 				UND ("Unknown tab");	break;
        }
        break;

      case 'h':			/* On */
      /* i,j,k are below */
      case 'l':			/* Off */
        {
          int on_p = (st->buf[st->idx-1] == 'h');
          switch (av[0]) {
          case 1:
            if (on_p)
              /* LOG ("Application cursor keys") */;
            else
              /* LOG ("Normal cursor keys") */;
            break;
          case 2:		UND ("USASCII sets G0-G3"); 		break;
				/* Or ANSI/VT52 Mode? */
          case 3:		
            st->lcf = False;
            if (on_p)		UND ("132 Column Mode");
            else		LOG ("80 Column Mode");
            break;
          case 4:		UND ("Smooth Scroll"); 			break;
          case 5:
            if (on_p)
              LOG ("Reverse Video on");
            else
              LOG ("Reverse Video off");
            tty->inverse_p = on_p;
            break;
          case 6:
            st->lcf = False;
            if (on_p)
              LOG ("Origin Mode on");
            else
              LOG ("Origin Mode off");
            st->origin_relative_p = on_p;
            tty->x = 0;
            tty->y = on_p ? st->scroll.y1 : 0;
            break;
          case 7:
            tty->state->auto_wrap_p = on_p;
            if (on_p)
              LOG ("Auto-wrap on");
            else
              LOG ("Auto-wrap off");
            if (!on_p)
              st->lcf = False;
            break;
          case 8:		UND ("Auto-Repeat Keys"); 		break;
          case 9:		UND ("Send Mouse"); 			break;
				/* Or Interlace? */
          case 10:		UND ("Show toolbar"); 			break;
          case 12:		UND ("Start blinking"); 		break;
          case 13:		UND ("Start blinking"); 		break;
          case 14:		UND ("Enable XOR"); 			break;
          case 18:		UND ("Print Form Feed"); 		break;
          case 19:		UND ("Print full screen"); 		break;

          case 20:
            if (on_p)		LOG ("Linefeed mode");
            else		LOG ("Newline mode");
            st->linefeed_p = on_p;
            break;

          case 25:		UND ("Show cursor"); 			break;
          case 30:		UND ("Show scrollbar"); 		break;
          case 35:		UND ("Enable font-shifting"); 		break;
          case 38:		UND ("Enter Tektronix"); 		break;
          case 40:		UND ("Allow 80/132"); 			break;
          case 41:		UND ("more"); 				break;
          case 42:		UND ("National replacement");		break;
          case 43:		UND ("Graphic Expanded Print"); 	break;
          case 44:		UND ("Graphic Print Color"); 		break;
          case 45:		UND ("Graphic Print Color Syntax"); 	break;
          case 46:		UND ("Graphic Print Background"); 	break;
          case 47:		UND ("Graphic Rotated Print"); 		break;
          case 66:	     /* UND ("Application keypad"); */	 	break;
          case 67:		UND ("Backarrow backspace"); 		break;
          case 69:		UND ("Margin mode"); 			break;
          case 80:		UND ("Sixel Mode"); 			break;
          case 95:		UND ("Do not clear"); 			break;
          case 1000:		UND ("Send Mouse"); 			break;
          case 1001:		UND ("Hilite Mouse Tracking"); 		break;
          case 1002:		UND ("Cell Motion Mouse"); 		break;
          case 1003:		UND ("All Motion Mouse"); 		break;
          case 1004:		UND ("Focus Events"); 			break;
          case 1005:		UND ("UTF-8 Mouse"); 			break;
          case 1006:		UND ("SGR Mouse"); 			break;
          case 1007:		UND ("Alternate Scroll"); 		break;
          case 1010:		UND ("Scroll on output"); 		break;
          case 1011:		UND ("Scroll on press"); 		break;
          case 1014:		UND ("Fast scroll"); 			break;
          case 1015:		UND ("URXVT Mouse"); 			break;
          case 1016:		UND ("SGR Mouse"); 			break;
          case 1024:		LOG ("Enable/disable focus report");	break;
          case 1034:		UND ("Meta key"); 			break;
          case 1035:		UND ("Alt modifiers"); 			break;
          case 1036:		UND ("ESC Meta"); 			break;
          case 1037:		UND ("DEL Delete"); 			break;
          case 1039:		UND ("ESC Alt"); 			break;
          case 1040:		UND ("selection when not highlighted"); break;
          case 1041:		UND ("Use CLIPBOARD"); 			break;
          case 1042:		UND ("Enable Urgency"); 		break;
          case 1043:		UND ("Enable raising"); 		break;
          case 1044:		UND ("Reuse clipboard"); 		break;
          case 1045:		UND ("Extended reverse-wraparound");	break;
          case 1046:		UND ("Enable Alternate Screen"); 	break;
          case 1047:		UND ("Alternate Screen Buffer"); 	break;
          case 1048:		UND ("Save cursor"); 			break;
          case 1049:		UND ("Save cursor"); 			break;
          case 1050:		UND ("Terminfo Fn keys"); 		break;
          case 1051:		UND ("Set Sun Fn keys"); 		break;
          case 1052:		UND ("HP Fn keys"); 			break;
          case 1053:		UND ("SCO Fn keys"); 			break;
          case 1060:		UND ("Legacy keyboard emulation");	break;
          case 1061:		UND ("VT220 keyboard emulation"); 	break;
          case 2001:		UND ("Readline mouse 1"); 		break;
          case 2002:		UND ("Readline mouse 2"); 		break;
          case 2003:		UND ("Readline mouse 2"); 		break;
          case 2004:		UND ("Bracketed paste"); 		break;
          case 2005:		UND ("readline quoting"); 		break;
          case 2006:		UND ("Readline newline paste"); 	break;
          default:		UND ("Unknown GR H/L");			break;
          }
        }
        break;

      case 'i':							   /* Ports */
        if (av[0] == UNDEF) av[0] = 0;
        switch (av[0]) {
        case 0:			UND ("Print screen");			break;
        case 1:			UND ("Print cursor line");		break;
        case 4:			UND ("Aux Port Off");			break;
        case 5:			UND ("Aux Port On");			break;
        case 10:		UND ("Print dpy");			break;
        case 11:		UND ("Print all");			break;
        default:		UND ("Unknown port request");		break;
        }
        break;

      /* Unused: 'j', 'k'; 'l' is above. */

      case 'm':					/* Select Graphic Rendition */
        if (ac == 0)
          av[ac++] = 0;  /* No args means "CSI 0m" */
        for (i = 0; i < ac; i++) {
          switch (av[i]) {
          case UNDEF:
          case 0:				LOG ("Reset text props");
            st->flags = 0;
            break;
          case 1:				LOG ("Bold");
            st->flags |= TTY_BOLD;
            break;
          case 2:				LOG ("Dim");
            st->flags |= TTY_DIM;
            break;
          case 3:				LOG ("Italic");
            st->flags |= TTY_ITALIC;
            break;
          case 4:				LOG ("Underline");
            st->flags |= TTY_UNDERLINE;
            break;
          case 5:				LOG ("Blink");
            st->flags |= TTY_BLINK;
            break;
          case 6:				LOG ("Blink fast");
            st->flags |= TTY_BLINK;
            break;
          case 7:				LOG ("Invert");
            st->flags |= TTY_INVERSE;
            break;
          case 8:	 			UND ("Hide");		break;
          case 9: 				UND ("Strike");		break;

          case 10:				LOG ("Primary font");
            st->flags &= ~TTY_SYMBOLS;
            break;
          case 11:				LOG ("Alternate font 1");
            st->flags |= TTY_SYMBOLS;
            break;
          case 12:	 	UND ("Alternate font 2"); 		break;
          case 13:	 	UND ("Alternate font 3"); 		break;
          case 14:	 	UND ("Alternate font 4"); 		break;
          case 15:	 	UND ("Alternate font 5"); 		break;
          case 16:	 	UND ("Alternate font 6"); 		break;
          case 17:	 	UND ("Alternate font 7"); 		break;
          case 18:	 	UND ("Alternate font 8"); 		break;
          case 19:	 	UND ("Alternate font 9"); 		break;
          case 20:	 	UND ("Fraktur font"); 			break;
          case 21:		LOG ("Doubly underlined");
            st->flags |= TTY_UNDERLINE;
            break;
          case 22:		LOG ("Not dim");
            st->flags &= ~TTY_DIM;
            break;
          case 23:		LOG ("Not italic");
            st->flags &= ~(TTY_ITALIC|TTY_BOLD);
            break;
          case 24:		LOG ("Not underlined");
            st->flags &= ~TTY_UNDERLINE;
            break;
          case 25:		LOG ("No blink");
            st->flags &= ~TTY_BLINK;
            break;
          case 26: 		LOG ("Proportional spacing"); 		break;
          case 27:		LOG ("Not reversed");
            st->flags &= ~TTY_INVERSE;
            break;
          case 28: 		LOG ("Reveal"); 			break;
          case 29: 		LOG ("Not strike"); 			break;

          case 30: 		LOG ("Set foreground color 1");
            set_color (tty, True, 0); 					break;
          case 31: 		LOG ("Set foreground color 2");
            set_color (tty, True, 1); 					break;
          case 32: 		LOG ("Set foreground color 3");
            set_color (tty, True, 2); 					break;
          case 33: 		LOG ("Set foreground color 4");
            set_color (tty, True, 3); 					break;
          case 34: 		LOG ("Set foreground color 5");
            set_color (tty, True, 4); 					break;
          case 35: 		LOG ("Set foreground color 6");
            set_color (tty, True, 5); 					break;
          case 36: 		LOG ("Set foreground color 7");
            set_color (tty, True, 6); 					break;
          case 37: 		LOG ("Set foreground color 8");
            set_color (tty, True, 7); 					break;

          case 38: 		LOG ("Set foreground color N");
            if (av[i] == 5)
              set_color (tty, True, av[i+1]);	/* 5;N */
            else if (av[i] == 2)
              {
                st->fg.r = av[i+1];		/* 2;R;G;B */
                st->fg.g = av[i+2];
                st->fg.b = av[i+3];
              }
            i = 999;  /* stop processing */
            break;

          case 39: 		LOG ("Default foreground color");
            set_color (tty, True, 0); 					break;

          case 40: 		LOG ("Set background color 1");
            set_color (tty, False, 0); 					break;
          case 41: 		LOG ("Set background color 2");
            set_color (tty, False, 1); 					break;
          case 42: 		LOG ("Set background color 3");
            set_color (tty, False, 2); 					break;
          case 43: 		LOG ("Set background color 4");
            set_color (tty, False, 3); 					break;
          case 44: 		LOG ("Set background color 5");
            set_color (tty, False, 4); 					break;
          case 45: 		LOG ("Set background color 6");
            set_color (tty, False, 5); 					break;
          case 46: 		LOG ("Set background color 7");
            set_color (tty, False, 6); 					break;
          case 47: 		LOG ("Set background color 8");
            set_color (tty, False, 7); 					break;

          case 48: 		LOG ("Set background color N");
            if (av[i] == 5)
              set_color (tty, False, av[i+1]);	/* 5;N */
            else if (av[i] == 2)
              {
                st->bg.r = av[i+1];		/* 2;R;G;B */
                st->bg.g = av[i+2];
                st->bg.b = av[i+3];
              }
            i = 999;  /* stop processing */
            break;

          case 49: 		LOG ("Default background color");
            set_color (tty, False, 0); 					break;

          case 50: 		UND ("Fixed width"); 			break;
          case 51: 		UND ("Framed"); 			break;
          case 52: 		UND ("Encircled"); 			break;
          case 53: 		UND ("Overlined"); 			break;
          case 54: 		UND ("Not framed"); 			break;
          case 55: 		UND ("Not overlined"); 			break;
          case 58: 		UND ("Set underline color"); 		break;
				/*  5;n or 2;r;g;b */
          case 59: 		UND ("Default underline color"); 	break;
          case 60: 		UND ("Ideogram underline"); 		break;
          case 61: 		UND ("Ideogram double underline"); 	break;
          case 62: 		UND ("Ideogram overline"); 		break;
          case 63: 		UND ("Ideogram double overline"); 	break;
          case 64: 		UND ("Ideogram stress marking"); 	break;
          case 65: 		UND ("No ideogram"); 			break;
          case 73: 		UND ("Superscript"); 			break;
          case 74: 		UND ("Subscript"); 			break;
          case 75: 		UND ("Not super/sub"); 			break;
          default: 		UND ("Unknown"); 			break;
          break;
          }
        }
        break;

      case 'n':							 /* Reports */
        switch (av[0]) {
        case 5:			LOG ("Report status");
          if (tty->tty_send)
            {			/* Terminal ok */
              const char *s = CSI "0n";
              (*tty->tty_send) (tty->closure, s);
            }
          break;
        case 6:			LOG ("Report cursor position");
          if (tty->tty_send)
            {
              char buf[40];
              sprintf (buf, CSI "%d;%dR", tty->y+1, tty->x+1);
              (*tty->tty_send) (tty->closure, buf);
            }
          break;
        default:		UND ("Unknown report request");		break;
        break;
        }
        break;

      /* No 'o' */

      case 'p':			UND ("Pointer mode");			break;
      case 'q':			UND ("LEDs");				break;

      case 'r':			LOG ("Scroll Region");
        if (av[0] == UNDEF) av[0] = 1;
        if (av[1] == UNDEF) av[1] = tty->height;
        /* Top and bottom lines, 1-based, inclusive.
           So "3;3" means a single line at y=2. */
        st->scroll.y1 = av[0] - 1;
        st->scroll.y2 = av[1];
        tty->x = 0;
        tty->y = st->scroll.y1;
        st->lcf = False;
        break;

      case 's':			LOG ("Save Current Cursor Position");
        st->saved.flags = st->flags;
        st->saved.lcf = st->lcf;
        st->saved.x = tty->x;
        st->saved.y = tty->y;
        break;

      case 't':			UND ("Window manip");			break;

      case 'u':			LOG ("Restore Saved Cursor Position");
        st->flags = st->saved.flags;
        st->lcf = st->saved.lcf;
        tty->x = st->saved.x;
        tty->y = st->saved.y;
        /* Set lcf to False here? */
        break;

      case 'v':			UND ("Display extent");			break;
      case 'w':			UND ("State report");			break;
      case 'x':			UND ("Param report");			break;

      case 'y':							   /* Tests */
        switch (av[0]) {
        case 2:
          if (av[1] == UNDEF) av[1] = 0;
          switch (av[1]) {
            case 0:		LOG ("Reset");
              ansi_tty_reset (tty);
              break;
            case 1:		LOG ("Self-test");
              if (tty->tty_send)
                {
                  char buf[40];
                  sprintf (buf, CSI "%dn", 0);  /* Ready */
                  (*tty->tty_send) (tty->closure, buf);
                }
              break;
            case 2:		UND ("Data loopback test");		break;
            case 4:		UND ("EIA modem test");			break;
            case 8:		UND ("Repeat tests forever");		break;
          default:		UND ("Unknown test");			break;
          }
          break;
        default:		UND ("Unknown y");			break;
        break;
        }
        break;

      case 'z':			UND ("Erase rect");			break;
      case '{':			UND ("Selective erase");		break;
      case '|':			UND ("Cols per page");			break;
      case '}':			UND ("Insert cols");			break;
      case '~':			UND ("Delete cols");			break;
      default:			UND ("Unknown"); 			break;
      }
    }


  /********************************************************************* Fe */


  else if (st->idx == 2 &&			      /* Fe escape sequence */
           st->buf[0] == ESC &&
           c >= 0x40 &&				      /*    @A-Z[\]^_       */
           c <= 0x5F)
    {
      kind = "Fe";
      done = True;

      switch (c) {				/* These commands await ST  */
      case 'P':					/*   Device Control String  */
      case ']':					/*   System Command         */
      case 'X':					/*   Start of String        */
      case '^':					/*   Privacy Message        */
      case '_':					/*   Application Program    */
        st->awaiting_st = True;
        break;
      case '\\':				/* ST: String Terminator    */
        st->awaiting_st = False;
        break;
      case 'N': UND ("Single Shift Two"); break;
      case 'O': UND ("Single Shift Three"); break;

      case '[':				/* CSI: Control Sequence Introducer */
        done = False;
        break;

      case 'A':					LOG ("VT52 Cursor up");
        tty->y--;
        break;
      case 'B':					LOG ("VT52 Cursor down");
        tty->y++;
        scrolled_p = True;
        /* Set lcf to False here? */
        break;
      case 'C':					LOG ("VT52 Cursor right");
        tty->x++;
        scrolled_p = True;
        /* Set lcf to False here? */
        break;
      case 'D':					LOG ("Cursor down");
        goto DO_LF;				/* VT52 Cursor left! */
        break;
      case 'E':					LOG ("Next line");
        tty->x = 0;
        goto DO_LF;
        break;
      case 'F':					LOG ("VT52 Graphics mode");
        st->flags |= TTY_SYMBOLS;
        break;
      case 'G':					LOG ("VT52 Exit graphics");
        st->flags &= ~TTY_SYMBOLS;
        break;
      case 'H':					LOG ("Set tab");
        st->tabs[tty->x] = 1;
	break;

      case 'I':  /* VT52 */
      case 'M':
        st->lcf = False;
        tty->y--;
        if (tty->y < st->scroll.y1)
          {
            LOG ("Rev LF scroll");
            tty->y = st->scroll.y1;
            tty_scroll (tty, -1);
            scrolled_p = True;
          }
        else
          LOG ("Rev LF");
        break;

      case 'J':					LOG ("VT52 Erase to EOS");
        tty_erase (tty,
                   tty->x, tty->x,
                   tty->width-1, tty->height-1);
        break;
      case 'K':					LOG ("VT52 Erase to EOL");
        tty_erase (tty,
                   tty->x, tty->y,
                   tty->width-1, tty->y);
        break;

      case 'Y':					UND ("VT52 Direct cursor");
        /* 4-byte sequence: "ESC Y y x" where x and y are \037 + N.
           But the rest of the Fe commands are 2 bytes. */
        break;

      case 'Z':					LOG ("VT52 Identify");
        if (tty->tty_send)
          {
            char buf[40];
            sprintf (buf, "%c%c%c", ESC, '/', 'Z');
            (*tty->tty_send) (tty->closure, buf);
          }
        break;

      default:					UND ("Unknown");	break;
      }
    }


  /********************************************************************* Fs */


  else if (st->idx == 2 &&			      /* Fs escape sequence */
           st->buf[0] == ESC &&
           c >= 0x60 &&				      /*    `a-z{|}~        */
           c <= 0x7E)
    {
      kind = "Fs";
      done = True;

      switch (c) {
      case '`':				UND ("Disable manual input"); 	break;
      case 'a':				UND ("Interrupt"); 		break;
      case 'b':				UND ("Enable manual input"); 	break;
      case 'c':				LOG ("Full Reset");
        ansi_tty_reset (tty);
        break;
      case 'd':				UND ("Coding method delimiter");break;
      case 'l':				UND ("Memory Lock"); 		break;
      case 'm':				UND ("Memory Unlock"); 		break;
      case 'n':				UND ("G2 as GL"); 		break;
      case 'o':				UND ("G3 as GL"); 		break;
      case '|':				UND ("G3 as GR"); 		break;
      case '~':				UND ("G1 as GR"); 		break;
      case '}':				UND ("G2 as GR"); 		break;
      default:				UND ("Unknown"); 		break;
      }
    }

  else if (st->idx > 0 &&
           st->buf[0] == ESC)
    {
      /* An ESC followed by a character that matched none of the above.
         We assume it is an unknown 2-byte sequence, which may not be true.
       */
      kind = "ESC";
      done = True;
      UND ("Unrecognized");
    }


  /******************************************************************* UTF8 */

  /* Assemble UTF-8 multi-byte sequences into a 32 bit Unicode character.   */


  else if (st->unicrud > 0)		/* Expect N more bytes of Unicode   */
    {
      kind = "UC";
      st->unicrud--;
      if (st->unicrud == 0)
        {
          /* Finished: replace c with the unichar. */
          c = 0;
          utf8_decode ((unsigned char *) st->buf, st->idx, &c);
          done = True;
          goto SELF_INSERT;
        }
    }
  else if ((c & 0xE0) == 0xC0)		/* 110xxxxx: 11 bits, 2 bytes      */
    {
      kind = "UC2";
      st->unicrud = 1;
    }
  else if ((c & 0xF0) == 0xE0)		/* 1110xxxx: 16 bits, 3 bytes      */
    {
      kind = "UC3";
      st->unicrud = 2;
    }
  else if ((c & 0xF8) == 0xF0)		/* 11110xxx: 21 bits, 4 bytes      */
    {
      kind = "UC4";
      st->unicrud = 3;
    }
  else if ((c & 0xFC) == 0xF8)		/* 111110xx: 26 bits, 5 bytes      */
    {
      kind = "UC5";
      st->unicrud = 4;
    }
  else if ((c & 0xFE) == 0xFC)		/* 1111110x: 31 bits, 6 bytes      */
    {
      kind = "UC6";
      st->unicrud = 5;
    }


  /******************************************************************** Co */


  else					/* Co control codes: single chars. */
    {
    SELF_INSERT:
      kind = "Co";
      done = True;

    SELF_INSERT_INTERLUDE:

      switch (c) {
      case 0:							/* NOOP    */
        break;

      case 0x07:						/* BEL     */
        tty_log_c (tty, c);
        break;

      case 0x08:						/* BS      */
        tty_log_c (tty, c);
        if (tty->x > 0)
          tty->x--;
        st->lcf = False;
        break;

      case 0x09:						/* HT: TAB */
        {
          /* Tabs are motion and do not alter what's under them. */
          tty_log_c (tty, c);
          tty->x++;
          while (tty->x < tty->width && !st->tabs[tty->x])
            tty->x++;
          st->lcf = False;
        }
        break;

      case 0x0A:						/* LF      */
      case 0x0B:						/* VT      */
      case 0x0C:						/* FF      */
        tty_log_c (tty, c);
      DO_LF:
        if (st->linefeed_p)
          tty->x = 0;
        tty->y++;
        st->lcf = False;
        scrolled_p = True;
        if (tty->y >= st->scroll.y2)
          {
            int n = tty->y - st->scroll.y2 + 1;
            ac = 0;
            av[ac++] = n;
            LOG ("Scroll");
            tty_scroll (tty, n);
            tty->y = st->scroll.y2 - 1;
          }
        break;

      case 0x1A:						/* SUB     */
        tty_log_c (tty, c);
        st->lcf = False;
        break;

      case 0x0D:						/* CR      */
        tty_log_c (tty, c);
        tty->x = 0;
        st->lcf = False;
        break;

      case 0x0E:						/* SO      */
        tty_log_c (tty, 0);
        LOG ("SO font G1");
        if (st->g1 & TTY_SYMBOLS)
          st->flags |= TTY_SYMBOLS;
        else
          st->flags &= ~TTY_SYMBOLS;
        break;

      case 0x0F:						/* SI      */
        tty_log_c (tty, 0);
        LOG ("SI font G0");
        if (st->g0 & TTY_SYMBOLS)
          st->flags |= TTY_SYMBOLS;
        else
          st->flags &= ~TTY_SYMBOLS;
        break;

      default:							/* Insert  */
        tty_log_c (tty, c);
        if (c >= ' ')
          {
            tty_char *ch;
            if (tty->x >= tty->width - 1)
              {
                tty->x = tty->width - 1;
                if (st->lcf == False)
                  st->lcf = True;
                else
                  {
                    st->lcf = False;
                    if (tty->state->auto_wrap_p)
                      {
                        tty->x = 0;
                        tty->y++;
                        if (tty->y >= st->scroll.y2)
                          {
                            int n = tty->y - st->scroll.y2 + 1;
                            ac = 0;
                            av[ac++] = n;
                            LOG ("Scroll wrap");
                            tty_scroll (tty, n);
                            tty->y = st->scroll.y2 - 1;
                            scrolled_p = True;
                          }
                        else if (tty->debug_p > 2)
                          LOG ("Wrap");
                      }
                  }
              }
            
            if (tty->x >= tty->width) abort();
            if (tty->y >= tty->height) abort();

            ch = &tty->grid[tty->y * tty->width + tty->x];
            ch->c     = c;
            ch->flags = st->flags;
            ch->fg    = st->fg;
            ch->bg    = st->bg;
            tty->x++;
          }
        break;
      }
    }

  if (tty->x < 0)           tty->x = 0;
  if (tty->x >= tty->width) tty->x = tty->width - 1;

  /* Scrolling commands, or insertions that changed Y, clip the cursor to
     the scrolling region. But cursor-positioning commands do not. */
  if (scrolled_p)
    {
      if (tty->y <  st->scroll.y1) tty->y = st->scroll.y1;
      if (tty->y >= st->scroll.y2) tty->y = st->scroll.y2 - 1;
    }
  else
    {
      if (tty->y <  0) tty->y = 0;
      if (tty->y >= tty->height) tty->y = tty->height - 1;
    }

  if (done)
    {
      st->idx    = 0;
      st->buf[0] = 0;
    }
}

