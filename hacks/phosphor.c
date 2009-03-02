/* xscreensaver, Copyright (c) 1999-2005 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Phosphor -- simulate a glass tty with long-sustain phosphor.
 * Written by Jamie Zawinski <jwz@jwz.org>
 * Pty and vt100 emulation by Fredrik Tolf <fredrik@dolda2000.com>
 */

#include "screenhack.h"

#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>

#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>

#ifdef HAVE_FORKPTY
# include <sys/ioctl.h>
# ifdef HAVE_PTY_H
#  include <pty.h>
# endif
# ifdef HAVE_UTIL_H
#  include <util.h>
# endif
#endif /* HAVE_FORKPTY */

extern XtAppContext app;

#define FUZZY_BORDER

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define BLANK  0
#define FLARE  1
#define NORMAL 2
#define FADE   3
#define STATE_MAX FADE

#define CURSOR_INDEX 128

#define NPAR 16

typedef struct {
  unsigned char name;
  int width, height;
  Pixmap pixmap;
#ifdef FUZZY_BORDER
  Pixmap pixmap2;
#endif /* FUZZY_BORDER */
  Bool blank_p;
} p_char;

typedef struct {
  p_char *p_char;
  int state;
  Bool changed;
} p_cell;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  XFontStruct *font;
  int grid_width, grid_height;
  int char_width, char_height;
  int saved_x, saved_y;
  int scale;
  int ticks;
  int mode;
  pid_t pid;
  int escstate;
  int csiparam[NPAR];
  int curparam;
  p_char **chars;
  p_cell *cells;
  XGCValues gcv;
  GC gc0;
  GC gc1;
#ifdef FUZZY_BORDER
  GC gc2;
#endif /* FUZZY_BORDER */
  GC *gcs;
  XImage *font_bits;

  int cursor_x, cursor_y;
  XtIntervalId cursor_timer;
  Time cursor_blink;

  FILE *pipe;
  XtInputId pipe_id;
  Bool input_available_p;
  Time subproc_relaunch_delay;
  XComposeStatus compose;
  Bool meta_sends_esc_p;
  Bool swap_bs_del_p;

} p_state;


static void capture_font_bits (p_state *state);
static p_char *make_character (p_state *state, int c);
static void drain_input (p_state *state);
static void char_to_pixmap (p_state *state, p_char *pc, int c);
static void launch_text_generator (p_state *state);


/* About font metrics:

   "lbearing" is the distance from the leftmost pixel of a character to
   the logical origin of that character.  That is, it is the number of
   pixels of the character which are to the left of its logical origin.

   "rbearing" is the distance from the logical origin of a character to
   the rightmost pixel of a character.  That is, it is the number of
   pixels of the character to the right of its logical origin.

   "descent" is the distance from the bottommost pixel of a character to
   the logical baseline.  That is, it is the number of pixels of the
   character which are below the baseline.

   "ascent" is the distance from the logical baseline to the topmost pixel.
   That is, it is the number of pixels of the character above the baseline.

   Therefore, the bounding box of the "ink" of a character is
   lbearing + rbearing by ascent + descent;

   "width" is the distance from the logical origin of this character to
   the position where the logical orgin of the next character should be
   placed.

   For our purposes, we're only interested in the part of the character
   lying inside the "width" box.  If the characters have ink outside of
   that box (the "charcell" box) then we're going to lose it.  Alas.
 */

static p_state *
init_phosphor (Display *dpy, Window window)
{
  int i;
  unsigned long flags;
  p_state *state = (p_state *) calloc (sizeof(*state), 1);
  char *fontname = get_string_resource ("font", "Font");
  XFontStruct *font;

  state->dpy = dpy;
  state->window = window;

  XGetWindowAttributes (dpy, window, &state->xgwa);
  XSelectInput (dpy, window, state->xgwa.your_event_mask | ExposureMask);

  state->meta_sends_esc_p = get_boolean_resource ("metaSendsESC", "Boolean");
  state->swap_bs_del_p    = get_boolean_resource ("swapBSDEL",    "Boolean");

  state->font = XLoadQueryFont (dpy, fontname);

  if (!state->font)
    {
      fprintf(stderr, "couldn't load font \"%s\"\n", fontname);
      state->font = XLoadQueryFont (dpy, "fixed");
    }
  if (!state->font)
    {
      fprintf(stderr, "couldn't load font \"fixed\"");
      exit(1);
    }

  font = state->font;
  state->scale = get_integer_resource ("scale", "Integer");
  state->ticks = STATE_MAX + get_integer_resource ("ticks", "Integer");
  state->escstate = 0;

  {
    char *s = get_string_resource ("mode", "Integer");
    state->mode = 0;
    if (!s || !*s || !strcasecmp (s, "pipe"))
      state->mode = 0;
    else if (!strcasecmp (s, "pty"))
      state->mode = 1;
    else
      fprintf (stderr, "%s: mode must be either `pipe' or `pty', not `%s'\n",
               progname, s);

#ifndef HAVE_FORKPTY
    fprintf (stderr, "%s: no pty support on this system; using -pipe mode.\n",
             progname);
    state->mode = 0;
#endif /* HAVE_FORKPTY */
  }

#if 0
  for (i = 0; i < font->n_properties; i++)
    if (font->properties[i].name == XA_FONT)
      printf ("font: %s\n", XGetAtomName(dpy, font->properties[i].card32));
#endif /* 0 */

  state->cursor_blink = get_integer_resource ("cursor", "Time");
  state->subproc_relaunch_delay =
    (1000 * get_integer_resource ("relaunch", "Time"));

  state->char_width  = font->max_bounds.width;
  state->char_height = font->max_bounds.ascent + font->max_bounds.descent;

  state->grid_width = state->xgwa.width / (state->char_width * state->scale);
  state->grid_height = state->xgwa.height /(state->char_height * state->scale);
  state->cells = (p_cell *) calloc (sizeof(p_cell),
                                    state->grid_width * state->grid_height);
  state->chars = (p_char **) calloc (sizeof(p_char *), 256);

  state->gcs = (GC *) calloc (sizeof(GC), state->ticks + 1);

  {
    int ncolors = MAX (0, state->ticks - 3);
    XColor *colors = (XColor *) calloc (ncolors, sizeof(XColor));
    int h1, h2;
    double s1, s2, v1, v2;

    unsigned long fg = get_pixel_resource ("foreground", "Foreground",
                                           state->dpy, state->xgwa.colormap);
    unsigned long bg = get_pixel_resource ("background", "Background",
                                           state->dpy, state->xgwa.colormap);
    unsigned long flare = get_pixel_resource ("flareForeground", "Foreground",
                                              state->dpy,state->xgwa.colormap);
    unsigned long fade = get_pixel_resource ("fadeForeground", "Foreground",
                                             state->dpy,state->xgwa.colormap);

    XColor start, end;

    start.pixel = fade;
    XQueryColor (state->dpy, state->xgwa.colormap, &start);

    end.pixel = bg;
    XQueryColor (state->dpy, state->xgwa.colormap, &end);

    /* Now allocate a ramp of colors from the main color to the background. */
    rgb_to_hsv (start.red, start.green, start.blue, &h1, &s1, &v1);
    rgb_to_hsv (end.red, end.green, end.blue, &h2, &s2, &v2);
    make_color_ramp (state->dpy, state->xgwa.colormap,
                     h1, s1, v1,
                     h2, s2, v2,
                     colors, &ncolors,
                     False, True, False);

    /* Adjust to the number of colors we actually got. */
    state->ticks = ncolors + STATE_MAX;

    /* Now, GCs all around.
     */
    state->gcv.font = font->fid;
    state->gcv.cap_style = CapRound;
#ifdef FUZZY_BORDER
    state->gcv.line_width = (int) (((long) state->scale) * 1.3);
    if (state->gcv.line_width == state->scale)
      state->gcv.line_width++;
#else /* !FUZZY_BORDER */
    state->gcv.line_width = (int) (((long) state->scale) * 0.9);
    if (state->gcv.line_width >= state->scale)
      state->gcv.line_width = state->scale - 1;
    if (state->gcv.line_width < 1)
      state->gcv.line_width = 1;
#endif /* !FUZZY_BORDER */

    flags = (GCForeground | GCBackground | GCCapStyle | GCLineWidth);

    state->gcv.background = bg;
    state->gcv.foreground = bg;
    state->gcs[BLANK] = XCreateGC (state->dpy, state->window, flags,
                                   &state->gcv);

    state->gcv.foreground = flare;
    state->gcs[FLARE] = XCreateGC (state->dpy, state->window, flags,
                                   &state->gcv);

    state->gcv.foreground = fg;
    state->gcs[NORMAL] = XCreateGC (state->dpy, state->window, flags,
                                    &state->gcv);

    for (i = 0; i < ncolors; i++)
      {
        state->gcv.foreground = colors[i].pixel;
        state->gcs[STATE_MAX + i] = XCreateGC (state->dpy, state->window,
                                               flags, &state->gcv);
      }
  }

  capture_font_bits (state);

  launch_text_generator (state);

  return state;
}


/* Re-query the window size and update the internal character grid if changed.
 */
static void
resize_grid (p_state *state)
{
  int ow = state->grid_width;
  int oh = state->grid_height;
  p_cell *ocells = state->cells;
  int x, y;

  XGetWindowAttributes (state->dpy, state->window, &state->xgwa);

  state->grid_width = state->xgwa.width   /(state->char_width  * state->scale);
  state->grid_height = state->xgwa.height /(state->char_height * state->scale);

  if (ow == state->grid_width &&
      oh == state->grid_height)
    return;

  state->cells = (p_cell *) calloc (sizeof(p_cell),
                                    state->grid_width * state->grid_height);

  for (y = 0; y < state->grid_height; y++)
    {
      for (x = 0; x < state->grid_width; x++)
        {
          p_cell *ncell = &state->cells [state->grid_width * y + x];
          if (x < ow && y < oh)
            *ncell = ocells [ow * y + x];
          ncell->changed = True;
        }
    }

  if (state->cursor_x >= state->grid_width)
    state->cursor_x = state->grid_width-1;
  if (state->cursor_y >= state->grid_height)
    state->cursor_y = state->grid_height-1;

  free (ocells);
}


static void
capture_font_bits (p_state *state)
{
  XFontStruct *font = state->font;
  int safe_width = font->max_bounds.rbearing - font->min_bounds.lbearing;
  int height = state->char_height;
  unsigned char string[257];
  int i;
  Pixmap p = XCreatePixmap (state->dpy, state->window,
                            (safe_width * 256), height, 1);

  for (i = 0; i < 256; i++)
    string[i] = (unsigned char) i;
  string[256] = 0;

  state->gcv.foreground = 0;
  state->gcv.background = 0;
  state->gc0 = XCreateGC (state->dpy, p,
                          (GCForeground | GCBackground),
                          &state->gcv);

  state->gcv.foreground = 1;
  state->gc1 = XCreateGC (state->dpy, p,
                          (GCFont | GCForeground | GCBackground |
                           GCCapStyle | GCLineWidth),
                          &state->gcv);

#ifdef FUZZY_BORDER
  {
    state->gcv.line_width = (int) (((long) state->scale) * 0.8);
    if (state->gcv.line_width >= state->scale)
      state->gcv.line_width = state->scale - 1;
    if (state->gcv.line_width < 1)
      state->gcv.line_width = 1;
    state->gc2 = XCreateGC (state->dpy, p,
                            (GCFont | GCForeground | GCBackground |
                             GCCapStyle | GCLineWidth),
                            &state->gcv);
  }
#endif /* FUZZY_BORDER */

  XFillRectangle (state->dpy, p, state->gc0, 0, 0, (safe_width * 256), height);

  for (i = 0; i < 256; i++)
    {
      if (string[i] < font->min_char_or_byte2 ||
          string[i] > font->max_char_or_byte2)
        continue;
      XDrawString (state->dpy, p, state->gc1,
                   i * safe_width, font->ascent,
                   (char *) (string + i), 1);
    }

  /* Draw the cursor. */
  XFillRectangle (state->dpy, p, state->gc1,
                  (CURSOR_INDEX * safe_width), 1,
                  (font->per_char
                   ? font->per_char['n'-font->min_char_or_byte2].width
                   : font->max_bounds.width),
                  font->ascent - 1);

#if 0
  XCopyPlane (state->dpy, p, state->window, state->gcs[FLARE],
              0, 0, (safe_width * 256), height, 0, 0, 1L);
  XSync(state->dpy, False);
#endif

  XSync (state->dpy, False);
  state->font_bits = XGetImage (state->dpy, p, 0, 0,
                                (safe_width * 256), height, ~0L, XYPixmap);
  XFreePixmap (state->dpy, p);

  for (i = 0; i < 256; i++)
    state->chars[i] = make_character (state, i);
  state->chars[CURSOR_INDEX] = make_character (state, CURSOR_INDEX);
}


static p_char *
make_character (p_state *state, int c)
{
  p_char *pc = (p_char *) malloc (sizeof (*pc));
  pc->name = (unsigned char) c;
  pc->width =  state->scale * state->char_width;
  pc->height = state->scale * state->char_height;
  char_to_pixmap (state, pc, c);
  return pc;
}


static void
char_to_pixmap (p_state *state, p_char *pc, int c)
{
  Pixmap p = 0;
  GC gc;
#ifdef FUZZY_BORDER
  Pixmap p2 = 0;
  GC gc2;
#endif /* FUZZY_BORDER */
  int from, to;
  int x1, y;

  XFontStruct *font = state->font;
  int safe_width = font->max_bounds.rbearing - font->min_bounds.lbearing;

  int width  = state->scale * state->char_width;
  int height = state->scale * state->char_height;

  if (c < font->min_char_or_byte2 ||
      c > font->max_char_or_byte2)
    goto DONE;

  gc = state->gc1;
  p = XCreatePixmap (state->dpy, state->window, width, height, 1);
  XFillRectangle (state->dpy, p, state->gc0, 0, 0, width, height);
#ifdef FUZZY_BORDER
  gc2 = state->gc2;
  p2 = XCreatePixmap (state->dpy, state->window, width, height, 1);
  XFillRectangle (state->dpy, p2, state->gc0, 0, 0, width, height);
#endif /* FUZZY_BORDER */

  from = safe_width * c;
  to =   safe_width * (c + 1);

#if 0
  if (c > 75 && c < 150)
    {
      printf ("\n=========== %d (%c)\n", c, c);
      for (y = 0; y < state->char_height; y++)
        {
          for (x1 = from; x1 < to; x1++)
            printf (XGetPixel (state->font_bits, x1, y) ? "* " : ". ");
          printf ("\n");
        }
    }
#endif

  pc->blank_p = True;
  for (y = 0; y < state->char_height; y++)
    for (x1 = from; x1 < to; x1++)
      if (XGetPixel (state->font_bits, x1, y))
        {
          int xoff = state->scale / 2;
          int x2;
          for (x2 = x1; x2 < to; x2++)
            if (!XGetPixel (state->font_bits, x2, y))
              break;
          x2--;
          XDrawLine (state->dpy, p, gc,
                     (x1 - from) * state->scale + xoff, y * state->scale,
                     (x2 - from) * state->scale + xoff, y * state->scale);
#ifdef FUZZY_BORDER
          XDrawLine (state->dpy, p2, gc2,
                     (x1 - from) * state->scale + xoff, y * state->scale,
                     (x2 - from) * state->scale + xoff, y * state->scale);
#endif /* FUZZY_BORDER */
          x1 = x2;
          pc->blank_p = False;
        }

  /*  if (pc->blank_p && c == CURSOR_INDEX)
    abort();*/

 DONE:
  pc->pixmap = p;
#ifdef FUZZY_BORDER
  pc->pixmap2 = p2;
#endif /* FUZZY_BORDER */
}


/* Managing the display. 
 */

static void cursor_on_timer (XtPointer closure, XtIntervalId *id);
static void cursor_off_timer (XtPointer closure, XtIntervalId *id);

static Bool
set_cursor_1 (p_state *state, Bool on)
{
  p_cell *cell = &state->cells[state->grid_width * state->cursor_y
                              + state->cursor_x];
  p_char *cursor = state->chars[CURSOR_INDEX];
  int new_state = (on ? NORMAL : FADE);

  if (cell->p_char != cursor)
    cell->changed = True;

  if (cell->state != new_state)
    cell->changed = True;

  cell->p_char = cursor;
  cell->state = new_state;
  return cell->changed;
}

static void
set_cursor (p_state *state, Bool on)
{
  if (set_cursor_1 (state, on))
    {
      if (state->cursor_timer)
        XtRemoveTimeOut (state->cursor_timer);
      state->cursor_timer = 0;
      cursor_on_timer (state, 0);
    }
}




static void
cursor_off_timer (XtPointer closure, XtIntervalId *id)
{
  p_state *state = (p_state *) closure;
  set_cursor_1 (state, False);
  state->cursor_timer = XtAppAddTimeOut (app, state->cursor_blink,
                                         cursor_on_timer, closure);
}

static void
cursor_on_timer (XtPointer closure, XtIntervalId *id)
{
  p_state *state = (p_state *) closure;
  set_cursor_1 (state, True);
  state->cursor_timer = XtAppAddTimeOut (app, 2 * state->cursor_blink,
                                         cursor_off_timer, closure);
}


static void
clear (p_state *state)
{
  int x, y;
  state->cursor_x = 0;
  state->cursor_y = 0;
  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        if (cell->state == FLARE || cell->state == NORMAL)
          {
            cell->state = FADE;
            cell->changed = True;
          }
      }
  set_cursor (state, True);
}


static void
decay (p_state *state)
{
  int x, y;
  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        if (cell->state == FLARE)
          {
            cell->state = NORMAL;
            cell->changed = True;
          }
        else if (cell->state >= FADE)
          {
            cell->state++;
            if (cell->state >= state->ticks)
              cell->state = BLANK;
            cell->changed = True;
          }
      }
}


static void
scroll (p_state *state)
{
  int x, y;

  for (x = 0; x < state->grid_width; x++)
    {
      p_cell *from = 0, *to = 0;
      for (y = 1; y < state->grid_height; y++)
        {
          from = &state->cells[state->grid_width * y     + x];
          to   = &state->cells[state->grid_width * (y-1) + x];

          if ((from->state == FLARE || from->state == NORMAL) &&
              !from->p_char->blank_p)
            {
              *to = *from;
              to->state = NORMAL;  /* should be FLARE?  Looks bad... */
            }
          else
            {
              if (to->state == FLARE || to->state == NORMAL)
                to->state = FADE;
            }

          to->changed = True;
        }

      to = from;
      if (to && (to->state == FLARE || to->state == NORMAL))
        {
          to->state = FADE;
          to->changed = True;
        }
    }
  set_cursor (state, True);
}


static void
print_char (p_state *state, int c)
{
  static char last_c = 0;
  static int bk;

  p_cell *cell = &state->cells[state->grid_width * state->cursor_y
			       + state->cursor_x];
  int i, start, end;

  /* Start the cursor fading (in case we don't end up overwriting it.) */
  if (cell->state == FLARE || cell->state == NORMAL)
    {
      cell->state = FADE;
      cell->changed = True;
    }
  
  if (state->pid)  /* Only interpret VT100 sequences if running in pty-mode.
                      It would be nice if we could just interpret them all
                      the time, but that would require subprocesses to send
                      CRLF line endings instead of bare LF, so that's no good.
                    */
    {
      switch (state->escstate)
	{
	case 0:
	  switch (c)
	    {
	    case 7: /* BEL */
	      /* Dummy case - we don't want the screensaver to beep */
              /* #### But maybe this should flash the screen? */
	      break;
	    case 8: /* BS */
	      if (state->cursor_x > 0)
		state->cursor_x--;
	      break;
	    case 9: /* HT */
	      if (state->cursor_x < state->grid_width - 8)
		{
		  state->cursor_x = (state->cursor_x & ~7) + 8;
		}
	      else
		{
		  state->cursor_x = 0;
		  if (state->cursor_y < state->grid_height - 1)
		    state->cursor_y++;
		  else
		    scroll (state);
		}
	      break;
	    case 10: /* LF */
	    case 11: /* VT */
	    case 12: /* FF */
	      if(last_c == 13)
		{
		  cell->state = NORMAL;
		  cell->p_char = state->chars[bk];
		  cell->changed = True;
		}
	      if (state->cursor_y < state->grid_height - 1)
		state->cursor_y++;
	      else
		scroll (state);
	      break;
	    case 13: /* CR */
	      state->cursor_x = 0;
	      cell = &state->cells[state->grid_width * state->cursor_y];
	      if((cell->p_char == NULL) || (cell->p_char->name == CURSOR_INDEX))
		bk = ' ';
	      else
		bk = cell->p_char->name;
	      break;
	    case 14: /* SO */
	    case 15: /* SI */
	      /* Dummy case - I don't want to load several fonts for
		 the maybe two programs world-wide that use that */
	      break;
	    case 24: /* CAN */
	    case 26: /* SUB */
	      /* Dummy case - these interrupt escape sequences, so
		 they don't do anything in this state */
	      break;
	    case 27: /* ESC */
	      state->escstate = 1;
	      break;
	    case 127: /* DEL */
	      /* Dummy case - this is supposed to be ignored */
	      break;
	    case 155: /* CSI */
	      state->escstate = 2;
	      for(i = 0; i < NPAR; i++)
		state->csiparam[i] = 0;
	      state->curparam = 0;
	      break;
	    default:
	      cell->state = FLARE;
	      cell->p_char = state->chars[c];
	      cell->changed = True;
	      state->cursor_x++;

	      if (c != ' ' && cell->p_char->blank_p)
		cell->p_char = state->chars[CURSOR_INDEX];

	      if (state->cursor_x >= state->grid_width - 1)
		{
		  state->cursor_x = 0;
		  if (state->cursor_y >= state->grid_height - 1)
		    scroll (state);
		  else
		    state->cursor_y++;
		}
	      break;
	    }
	  break;
	case 1:
	  switch (c)
	    {
	    case 24: /* CAN */
	    case 26: /* SUB */
	      state->escstate = 0;
	      break;
	    case 'c': /* Reset */
	      clear (state);
	      state->escstate = 0;
	      break;
	    case 'D': /* Linefeed */
	      if (state->cursor_y < state->grid_height - 1)
		state->cursor_y++;
	      else
		scroll (state);
	      state->escstate = 0;
	      break;
	    case 'E': /* Newline */
	      state->cursor_x = 0;
	      state->escstate = 0;
	      break;
	    case 'M': /* Reverse newline */
	      if (state->cursor_y > 0)
		state->cursor_y--;
	      state->escstate = 0;
	      break;
	    case '7': /* Save state */
	      state->saved_x = state->cursor_x;
	      state->saved_y = state->cursor_y;
	      state->escstate = 0;
	      break;
	    case '8': /* Restore state */
	      state->cursor_x = state->saved_x;
	      state->cursor_y = state->saved_y;
	      state->escstate = 0;
	      break;
	    case '[': /* CSI */
	      state->escstate = 2;
	      for(i = 0; i < NPAR; i++)
		state->csiparam[i] = 0;
	      state->curparam = 0;
	      break;
	    case '%': /* Select charset */
	      /* No, I don't support UTF-8, since the phosphor font
		 isn't even Unicode anyway. We must still catch the
		 last byte, though. */
	    case '(':
	    case ')':
	      /* I don't support different fonts either - see above
		 for SO and SI */
	      state->escstate = 3;
	      break;
	    default:
	      /* Escape sequences not supported:
	       * 
	       * H - Set tab stop
	       * Z - Terminal identification
	       * > - Keypad change
	       * = - Other keypad change
	       * ] - OS command
	       */
	      state->escstate = 0;
	      break;
	    }
	  break;
	case 2:
	  switch (c)
	    {
	    case 24: /* CAN */
	    case 26: /* SUB */
	      state->escstate = 0;
	      break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
	      if (state->curparam < NPAR)
		state->csiparam[state->curparam] = (state->csiparam[state->curparam] * 10) + (c - '0');
	      break;
	    case ';':
	      state->csiparam[++state->curparam] = 0;
	      break;
	    case '[':
	      state->escstate = 3;
	      break;
	    case '@':
	      for (i = 0; i < state->csiparam[0]; i++)
		{
		  if(++state->cursor_x > state->grid_width)
		    {
		      state->cursor_x = 0;
		      if (state->cursor_y < state->grid_height - 1)
			state->cursor_y++;
		      else
			scroll (state);
		    }
		  cell = &state->cells[state->grid_width * state->cursor_y + state->cursor_x];
		  if (cell->state == FLARE || cell->state == NORMAL)
		    {
		      cell->state = FADE;
		      cell->changed = True;
		    }
		}
	      state->escstate = 0;
	      break;
	    case 'F':
	      state->cursor_x = 0;
	    case 'A':
	      if (state->csiparam[0] == 0)
		state->csiparam[0] = 1;
	      if ((state->cursor_y -= state->csiparam[0]) < 0)
		state->cursor_y = 0;
	      state->escstate = 0;
	      break;
	    case 'E':
	      state->cursor_x = 0;
	    case 'e':
	    case 'B':
	      if (state->csiparam[0] == 0)
		state->csiparam[0] = 1;
	      if ((state->cursor_y += state->csiparam[0]) >= state->grid_height - 1)
		state->cursor_y = state->grid_height - 1;
	      state->escstate = 0;
	      break;
	    case 'a':
	    case 'C':
	      if (state->csiparam[0] == 0)
		state->csiparam[0] = 1;
	      if ((state->cursor_x += state->csiparam[0]) >= state->grid_width - 1)
		state->cursor_x = state->grid_width - 1;
	      state->escstate = 0;
	      break;
	    case 'D':
	      if (state->csiparam[0] == 0)
		state->csiparam[0] = 1;
	      if ((state->cursor_x -= state->csiparam[0]) < 0)
		state->cursor_x = 0;
	      state->escstate = 0;
	      break;
	    case 'd':
	      if ((state->cursor_y = (state->csiparam[0] - 1)) >= state->grid_height - 1)
		state->cursor_y = state->grid_height - 1;
	      state->escstate = 0;
	      break;
	    case '`':
	    case 'G':
	      if ((state->cursor_x = (state->csiparam[0] - 1)) >= state->grid_width - 1)
		state->cursor_x = state->grid_width - 1;
	      state->escstate = 0;
	      break;
	    case 'f':
	    case 'H':
	      if ((state->cursor_y = (state->csiparam[0] - 1)) >= state->grid_height - 1)
		state->cursor_y = state->grid_height - 1;
	      if ((state->cursor_x = (state->csiparam[1] - 1)) >= state->grid_width - 1)
		state->cursor_x = state->grid_width - 1;
	      if(state->cursor_y < 0)
		state->cursor_y = 0;
	      if(state->cursor_x < 0)
		state->cursor_x = 0;
	      state->escstate = 0;
	      break;
	    case 'J':
	      start = 0;
	      end = state->grid_height * state->grid_width;
	      if (state->csiparam[0] == 0)
		start = state->grid_width * state->cursor_y + state->cursor_x;
	      if (state->csiparam[0] == 1)
		end = state->grid_width * state->cursor_y + state->cursor_x;
	      for (i = start; i < end; i++)
		{
		  cell = &state->cells[i];
		  if (cell->state == FLARE || cell->state == NORMAL)
		    {
		      cell->state = FADE;
		      cell->changed = True;
		    }
		}
	      set_cursor (state, True);
	      state->escstate = 0;
	      break;
	    case 'K':
	      start = 0;
	      end = state->grid_width;
	      if (state->csiparam[0] == 0)
		start = state->cursor_x;
	      if (state->csiparam[1] == 1)
		end = state->cursor_x;
	      for (i = start; i < end; i++)
		{
		  if (cell->state == FLARE || cell->state == NORMAL)
		    {
		      cell->state = FADE;
		      cell->changed = True;
		    }
		  cell++;
		}
	      state->escstate = 0;
	      break;
	    case 's': /* Save position */
	      state->saved_x = state->cursor_x;
	      state->saved_y = state->cursor_y;
	      state->escstate = 0;
	      break;
	    case 'u': /* Restore position */
	      state->cursor_x = state->saved_x;
	      state->cursor_y = state->saved_y;
	      state->escstate = 0;
	      break;
	    case '?': /* DEC Private modes */
	      if ((state->curparam != 0) || (state->csiparam[0] != 0))
		state->escstate = 0;
	      break;
	    default:
	      /* Known unsupported CSIs:
	       *
	       * L - Insert blank lines
	       * M - Delete lines (I don't know what this means...)
	       * P - Delete characters
	       * X - Erase characters (difference with P being...?)
	       * c - Terminal identification
	       * g - Clear tab stop(s)
	       * h - Set mode (Mainly due to its complexity and lack of good
                     docs)
	       * l - Clear mode
	       * m - Set mode (Phosphor is, per defenition, green on black)
	       * n - Status report
	       * q - Set keyboard LEDs
	       * r - Set scrolling region (too exhausting - noone uses this,
                     right?)
	       */
	      state->escstate = 0;
	      break;
	    }
	  break;
	case 3:
	  state->escstate = 0;
	  break;
	}
      set_cursor (state, True);
    }
  else
    {
      if (c == '\t') c = ' ';   /* blah. */

      if (c == '\r' || c == '\n')  /* handle CR, LF, or CRLF as "new line". */
	{
	  if (c == '\n' && last_c == '\r')
	    ;   /* CRLF -- do nothing */
	  else
	    {
	      state->cursor_x = 0;
	      if (state->cursor_y == state->grid_height - 1)
		scroll (state);
	      else
		state->cursor_y++;
	    }
	}
      else if (c == '\014')
	{
	  clear (state);
	}
      else
	{
	  cell->state = FLARE;
	  cell->p_char = state->chars[c];
	  cell->changed = True;
	  state->cursor_x++;

	  if (c != ' ' && cell->p_char->blank_p)
	    cell->p_char = state->chars[CURSOR_INDEX];

	  if (state->cursor_x >= state->grid_width - 1)
	    {
	      state->cursor_x = 0;
	      if (state->cursor_y >= state->grid_height - 1)
		scroll (state);
	      else
		state->cursor_y++;
	    }
	}
      set_cursor (state, True);
    }

  last_c = c;
}


static void
update_display (p_state *state, Bool changed_only)
{
  int x, y;

  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        int width, height, tx, ty;

        if (changed_only && !cell->changed)
          continue;

        width = state->char_width * state->scale;
        height = state->char_height * state->scale;
        tx = x * width;
        ty = y * height;

        if (cell->state == BLANK || cell->p_char->blank_p)
          {
            XFillRectangle (state->dpy, state->window, state->gcs[BLANK],
                            tx, ty, width, height);
          }
        else
          {
#ifdef FUZZY_BORDER
            GC gc1 = state->gcs[cell->state];
            GC gc2 = ((cell->state + 2) < state->ticks
                      ? state->gcs[cell->state + 2]
                      : 0);
            GC gc3 = (gc2 ? gc2 : gc1);
            if (gc3)
              XCopyPlane (state->dpy, cell->p_char->pixmap, state->window, gc3,
                          0, 0, width, height, tx, ty, 1L);
            if (gc2)
              {
                XSetClipMask (state->dpy, gc1, cell->p_char->pixmap2);
                XSetClipOrigin (state->dpy, gc1, tx, ty);
                XFillRectangle (state->dpy, state->window, gc1,
                                tx, ty, width, height);
                XSetClipMask (state->dpy, gc1, None);
              }
#else /* !FUZZY_BORDER */

            XCopyPlane (state->dpy,
                        cell->p_char->pixmap, state->window,
                        state->gcs[cell->state],
                        0, 0, width, height, tx, ty, 1L);

#endif /* !FUZZY_BORDER */
          }

        cell->changed = False;
      }
}


static void
run_phosphor (p_state *state)
{
  update_display (state, True);
  decay (state);
  drain_input (state);
}


/* Subprocess.
 */

static void
subproc_cb (XtPointer closure, int *source, XtInputId *id)
{
  p_state *state = (p_state *) closure;
  state->input_available_p = True;
}


static void
launch_text_generator (p_state *state)
{
  char buf[255];
  char *oprogram = get_string_resource ("program", "Program");
  char *program = (char *) malloc (strlen (oprogram) + 50);

  /* oprogram contains a "%d" where the current number of columns goes
   */
  strcpy (program, "( ");
  sprintf (program + strlen(program), oprogram, state->grid_width-1);
  strcat (program, " ) 2>&1");

#ifdef HAVE_FORKPTY
  if(state->mode == 1)
    {
      int fd;
      struct winsize ws;
      
      ws.ws_row = state->grid_height - 1;
      ws.ws_col = state->grid_width  - 2;
      ws.ws_xpixel = state->xgwa.width;
      ws.ws_ypixel = state->xgwa.height;
      
      state->pipe = NULL;
      if((state->pid = forkpty(&fd, NULL, NULL, &ws)) < 0)
	{
          /* Unable to fork */
          sprintf (buf, "%.100s: forkpty", progname);
	  perror(buf);
	}
      else if(!state->pid)
	{
          /* This is the child fork. */
          char *av[10];
          int i = 0;
	  if (putenv("TERM=vt100"))
            abort();
          av[i++] = "/bin/sh";
          av[i++] = "-c";
          av[i++] = program;
          av[i] = 0;
          execvp (av[0], av);
          sprintf (buf, "%.100s: %.100s", progname, oprogram);
	  perror(buf);
	  exit(1);
	}
      else
	{
          /* This is the parent fork. */
	  state->pipe = fdopen(fd, "r+");
	  state->pipe_id =
	    XtAppAddInput (app, fileno (state->pipe),
			   (XtPointer) (XtInputReadMask | XtInputExceptMask),
			   subproc_cb, (XtPointer) state);
	}
    }
  else
#endif /* HAVE_FORKPTY */
    {
      /* don't mess up controlling terminal if someone dumbly does
         "-pipe -program tcsh". */
      fclose (stdin);

      if ((state->pipe = popen (program, "r")))
	{
	  state->pipe_id =
	    XtAppAddInput (app, fileno (state->pipe),
			   (XtPointer) (XtInputReadMask | XtInputExceptMask),
			   subproc_cb, (XtPointer) state);
	}
      else
	{
          sprintf (buf, "%.100s: %.100s", progname, program);
	  perror (buf);
	}
    }

  free (program);
}


static void
relaunch_generator_timer (XtPointer closure, XtIntervalId *id)
{
  p_state *state = (p_state *) closure;
  launch_text_generator (state);
}


static void
drain_input (p_state *state)
{
  if (state->input_available_p)
    {
      unsigned char s[2];
      int n = read (fileno (state->pipe), (void *) s, 1);
      if (n == 1)
        {
          print_char (state, s[0]);
        }
      else
        {
          XtRemoveInput (state->pipe_id);
          state->pipe_id = 0;
	  if (state->pid)
	    {
	      waitpid(state->pid, NULL, 0);
	      fclose (state->pipe);
	    }
	  else
	    {
	      pclose (state->pipe);
	    }
          state->pipe = 0;

          if (state->cursor_x != 0)	/* break line if unbroken */
            print_char (state, '\n');	/* blank line */
          print_char (state, '\n');

          /* Set up a timer to re-launch the subproc in a bit. */
          XtAppAddTimeOut (app, state->subproc_relaunch_delay,
                           relaunch_generator_timer,
                           (XtPointer) state);
        }
        
      state->input_available_p = False;
    }
}


/* The interpretation of the ModN modifiers is dependent on what keys
   are bound to them: Mod1 does not necessarily mean "meta".  It only
   means "meta" if Meta_L or Meta_R are bound to it.  If Meta_L is on
   Mod5, then Mod5 is the one that means Meta.  Oh, and Meta and Alt
   aren't necessarily the same thing.  Icepicks in my forehead!
 */
static unsigned int
do_icccm_meta_key_stupidity (Display *dpy)
{
  unsigned int modbits = 0;
  int i, j, k;
  XModifierKeymap *modmap = XGetModifierMapping (dpy);
  for (i = 3; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      {
        int code = modmap->modifiermap[i * modmap->max_keypermod + j];
        KeySym *syms;
        int nsyms = 0;
        if (code == 0) continue;
        syms = XGetKeyboardMapping (dpy, code, 1, &nsyms);
        for (k = 0; k < nsyms; k++)
          if (syms[k] == XK_Meta_L || syms[k] == XK_Meta_R ||
              syms[k] == XK_Alt_L  || syms[k] == XK_Alt_R)
            modbits |= (1 << i);
        XFree (syms);
      }
  XFreeModifiermap (modmap);
  return modbits;
}

/* Returns a mask of the bit or bits of a KeyPress event that mean "meta". 
 */
static unsigned int
meta_modifier (Display *dpy)
{
  static Bool done_once = False;
  static unsigned int mask = 0;
  if (!done_once)
    {
      /* Really, we are supposed to recompute this if a KeymapNotify
         event comes in, but fuck it. */
      done_once = True;
      mask = do_icccm_meta_key_stupidity (dpy);
    }
  return mask;
}


static void
handle_events (p_state *state)
{
  XSync (state->dpy, False);
  while (XPending (state->dpy))
    {
      XEvent event;
      XNextEvent (state->dpy, &event);

      if (event.xany.type == ConfigureNotify)
        {
          resize_grid (state);

# if defined(HAVE_FORKPTY) && defined(TIOCSWINSZ)
          if (state->pid)
            {
              /* Tell the sub-process that the screen size has changed. */
              struct winsize ws;
              ws.ws_row = state->grid_height - 1;
              ws.ws_col = state->grid_width  - 2;
              ws.ws_xpixel = state->xgwa.width;
              ws.ws_ypixel = state->xgwa.height;
              ioctl (fileno (state->pipe), TIOCSWINSZ, &ws);
              kill (state->pid, SIGWINCH);
            }
# endif /* HAVE_FORKPTY && TIOCSWINSZ */
        }
      else if (event.xany.type == Expose)
        {
          update_display (state, False);
        }
      else if (event.xany.type == KeyPress)
        {
          KeySym keysym;
          unsigned char c = 0;
          XLookupString (&event.xkey, (char *) &c, 1, &keysym,
                         &state->compose);
          if (c != 0 && state->pipe)
            {
              if (!state->swap_bs_del_p) ;
              else if (c == 127) c = 8;
              else if (c == 8)   c = 127;

              /* If meta was held down, send ESC, or turn on the high bit. */
              if (event.xkey.state & meta_modifier (state->dpy))
                {
                  if (state->meta_sends_esc_p)
                    fputc ('\033', state->pipe);
                  else
                    c |= 0x80;
                }

              fputc (c, state->pipe);
              fflush (state->pipe);
              event.xany.type = 0;  /* don't interpret this event defaultly. */
            }
        }

      screenhack_handle_event (state->dpy, &event);
    }

  if (XtAppPending (app) & (XtIMTimer|XtIMAlternateInput))
    XtAppProcessEvent (app, XtIMTimer|XtIMAlternateInput);
}



char *progclass = "Phosphor";

char *defaults [] = {
  ".background:		   Black",
  ".foreground:		   Green",
  "*fadeForeground:	   DarkGreen",
  "*flareForeground:	   White",
  "*font:		   fixed",
  "*scale:		   6",
  "*ticks:		   20",
  "*delay:		   50000",
  "*cursor:		   333",
  "*program:		   xscreensaver-text --cols %d",
  "*relaunch:		   5",
  "*metaSendsESC:	   True",
  "*swapBSDEL:		   True",
#ifdef HAVE_FORKPTY
  "*mode:                  pty",
#else  /* !HAVE_FORKPTY */
  "*mode:                  pipe",
#endif /* !HAVE_FORKPTY */
  0
};

XrmOptionDescRec options [] = {
  { "-font",		".font",		XrmoptionSepArg, 0 },
  { "-scale",		".scale",		XrmoptionSepArg, 0 },
  { "-ticks",		".ticks",		XrmoptionSepArg, 0 },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-program",		".program",		XrmoptionSepArg, 0 },
  { "-pty",		".mode",		XrmoptionNoArg, "pty"   },
  { "-pipe",		".mode",		XrmoptionNoArg, "pipe"  },
  { "-meta",		".metaSendsESC",	XrmoptionNoArg, "False" },
  { "-esc",		".metaSendsESC",	XrmoptionNoArg, "True"  },
  { "-bs",		".swapBSDEL",		XrmoptionNoArg, "False" },
  { "-del",		".swapBSDEL",		XrmoptionNoArg, "True"  },
  { 0, 0, 0, 0 }
};


void
screenhack (Display *dpy, Window window)
{
  int delay = get_integer_resource ("delay", "Integer");
  p_state *state = init_phosphor (dpy, window);

  clear (state);

  while (1)
    {
      run_phosphor (state);
      XSync (dpy, False);
      handle_events (state);
      if (delay) usleep (delay);
    }
}
