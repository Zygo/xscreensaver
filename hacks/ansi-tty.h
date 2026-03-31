/* xscreensaver, Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __XSCREENSAVER_ANSI_TTY_H__
#define __XSCREENSAVER_ANSI_TTY_H__

typedef enum {
  TTY_BOLD      = 1,
  TTY_ITALIC    = 2,
  TTY_INVERSE   = 4,
  TTY_DIM       = 8,
  TTY_UNDERLINE = 16,
  TTY_BLINK     = 32,
  TTY_SYMBOLS   = 64,
} tty_flag;

typedef struct { unsigned char r, g, b; } tty_color;

typedef struct {
  unsigned long c;			/* Unicode */
  tty_flag flags;			/* Bitmask */
  tty_color fg, bg;
} tty_char;

typedef void (*tty_send_fn) (void *closure, const char *text);

typedef struct tty_state tty_state;
typedef struct {			/* Do not change these: */

  int x, y;				/*   Cursor position */
  int width, height;			/*   Screen size */
  int inverse_p;			/*   Flip the sense of TTY_INVERSE */
  tty_char *grid;			/*   Characters */
  tty_state *state;			/*   Internal state */

					/* You can change these: */

  tty_send_fn tty_send;			/*   Type characters to upstream */
  void *closure;
  tty_color cmap[16][2];		/*   Default indexed colors */

  int debug_p;				/*   1: log errors to stderr
					     2: and all commands
					     3: and all text
					     4: log to /tmp */
} ansi_tty;

extern ansi_tty *ansi_tty_init (int w, int h);
extern void ansi_tty_resize (ansi_tty *, int w, int h);
extern void ansi_tty_print (ansi_tty *, unsigned long c);
extern void ansi_tty_free (ansi_tty *);

extern const unsigned long ansi_graphics_unicode[256];

#endif /* __XSCREENSAVER_ANSI_TTY_H__ */
