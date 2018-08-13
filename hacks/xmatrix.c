/* xscreensaver, Copyright (c) 1999-2018 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Matrix -- simulate the text scrolls from the movie "The Matrix".
 *
 * The movie people distribute their own Windows/Mac screensaver that does
 * a similar thing, so I wrote one for Unix.  However, that version (the
 * Windows/Mac version at http://www.whatisthematrix.com/) doesn't match
 * what the computer screens in the movie looked like, so my `xmatrix' does
 * things differently.
 *
 * See also my `glmatrix' program, which does a 3D rendering of the similar
 * effect that appeared in the title sequence of the movies.
 *
 *
 *     ==========================================================
 *
 *         NOTE:
 *
 *         People just love to hack on this one.  I get sent
 *         patches to this all the time saying, ``here, I made
 *         it better!''  Mostly this hasn't been true.
 *
 *         If you've made changes to xmatrix, when you send me
 *         your patch, please explain, in English, both *what*
 *         your changes are, and *why* you think those changes
 *         make this screensaver behave more like the displays
 *         in the movie did.  I'd rather not have to read your
 *         diffs to try and figure that out for myself...
 *
 *         In particular, note that the characters in the movie
 *         were, in fact, low resolution and somewhat blurry/
 *         washed out.  They also definitely scrolled a
 *         character at a time, not a pixel at a time.
 *
 *         And keep in mind that this program emulates the
 *         behavior of the computer screens that were visible
 *         in the movies -- not the behavior of the effects in
 *         the title sequences.  "GLMatrix" does that.
 *
 *     ==========================================================
 *
 */

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "screenhack.h"
#include "textclient.h"
#include "ximage-loader.h"
#include <stdio.h>
#include <sys/wait.h>

#ifndef HAVE_JWXYZ
# include <X11/Intrinsic.h>
#endif

#include "images/gen/matrix1_png.h"
#include "images/gen/matrix2_png.h"
#include "images/gen/matrix1b_png.h"
#include "images/gen/matrix2b_png.h"

#define CHAR_COLS 16
#define CHAR_ROWS 13
#define CHAR_MAPS 3
#define PLAIN_MAP 1
#define GLOW_MAP  2

typedef struct {
           int glow    : 8;
  unsigned int glyph   : 9;  /* note: 9 bit characters! */
  unsigned int changed : 1;
  unsigned int spinner : 1;
} m_cell;

typedef struct {
  int pipe_loc;
  int remaining;
  int throttle;
  int y;
} m_feeder;

#define countof(x) (sizeof(x)/sizeof(*(x)))

static const int matrix_encoding[] = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                       192, 193, 194, 195, 196, 197, 198, 199,
                                       200, 201, 202, 203, 204, 205, 206, 207 };
static const int decimal_encoding[] = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
static const int hex_encoding[]     = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                        33, 34, 35, 36, 37, 38 };
static const int binary_encoding[] = { 16, 17 };
static const int dna_encoding[]    = { 33, 35, 39, 52 };
static const int ascii_encoding[]  = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127
};
static const unsigned char char_map[256] = {
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  /*   0 */
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  /*  16 */
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,  /*  32 */
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,  /*  48 */
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,  /*  64 */
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,  /*  80 */
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,  /*  96 */
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,  /* 112 */
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  /* 128 */
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  /* 144 */
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,  /* 160 */
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,  /* 176 */
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,  /* 192 */
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,  /* 208 */
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,  /* 224 */
  176,177,178,195,180,181,182,183,184,185,186,187,188,189,190,191   /* 240 */
};

#define CURSOR_GLYPH 97

/* larger numbers should mean more variability between columns */
#define BUF_SIZE 200

typedef enum { DRAIN_TRACE_A,
               TRACE_TEXT_A,		/* Call trans opt: received. */
               TRACE_A,			/* (31_) 5__-0_9_ */
               TRACE_DONE,

               DRAIN_TRACE_B,
               TRACE_TEXT_B,		/* Call trans opt: received. */
               TRACE_B,
               TRACE_FAIL,		/* System Failure */

               DRAIN_KNOCK,
               KNOCK,			/* Wake up, Neo... */

               DRAIN_NMAP,
               NMAP,			/* Starting nmap V. 2.54BETA25 */

               DRAIN_MATRIX,
               MATRIX,
               DNA,
               BINARY,
               DEC,
               HEX,
               ASCII } m_mode;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  GC draw_gc, erase_gc, scratch_gc;
  int grid_width, grid_height;
  int char_width, char_height;
  m_cell *cells;
  m_cell *background;
  m_feeder *feeders;
  int nspinners;
  Bool knock_knock_p;
  Bool small_p;
  Bool insert_top_p, insert_bottom_p;
  Bool use_pipe_p;
  m_mode mode;
  m_mode def_mode; /* Mode to return to after trace etc. */

  text_data *tc;
  char buf [BUF_SIZE*2+1]; /* ring buffer */
  Bool do_fill_buff;
  int buf_done;
  int buf_pos;
  Bool start_reveal_back_p; /* start reveal process for pipe */
  Bool back_text_full_p; /* is the pipe buffer (background) full ? */
  char back_line [BUF_SIZE*2+1]; /* line buffer for background */
  int back_pos; /* background line buffer position */
  int back_y;

  signed char *tracing;
  int density;

  const char *typing;
  Bool typing_scroll_p;
  Bool typing_cursor_p;
  Bool typing_bold_p;
  Bool typing_stutter_p;
  int typing_left_margin;
  int typing_char_delay;
  int typing_line_delay;
  int typing_delay;

  Bool cursor_on;
  int cursor_x, cursor_y;
  XtIntervalId cursor_timer;

  Pixmap images[CHAR_MAPS];
  int image_width, image_height;
  Bool images_flipped_p;

  int nglyphs;
  const int *glyph_map;

  unsigned long colors[5];
  int delay;
} m_state;


static void
load_images_1 (Display *dpy, m_state *state, int which)
{
  const unsigned char *png = 0;
  unsigned long size = 0;
  if (which == 1)
    {
      if (state->small_p)
        png = matrix1b_png, size = sizeof(matrix1b_png);
      else
        png = matrix1_png, size = sizeof(matrix1_png);
    }
  else
    {
      if (state->small_p)
        png = matrix2b_png, size = sizeof(matrix2b_png);
      else
        png = matrix2_png, size = sizeof(matrix2_png);
    }
  state->images[which] =
    image_data_to_pixmap (state->dpy, state->window, png, size,
                          &state->image_width, &state->image_height, 0);
}


static void
load_images (Display *dpy, m_state *state)
{
  load_images_1 (dpy, state, 1);
  load_images_1 (dpy, state, 2);
}


static void
flip_images_1 (m_state *state, int which)
{
  XImage *im = XGetImage (state->dpy, state->images[which], 0, 0,
                          state->image_width, state->image_height,
                          ~0L, (state->xgwa.depth > 1 ? ZPixmap : XYPixmap));
  int x, y, xx;
  int ww = state->char_width;
  unsigned long *row = (unsigned long *) malloc (sizeof(*row) * ww);

  for (y = 0; y < state->image_height; y++)
    {
      for (x = 0; x < CHAR_COLS; x++)
        {
          for (xx = 0; xx < ww; xx++)
            row[xx] = XGetPixel (im, (x * ww) + xx, y);
          for (xx = 0; xx < ww; xx++)
            XPutPixel (im, (x * ww) + xx, y, row[ww - xx - 1]);
        }
    }

  XPutImage (state->dpy, state->images[which], state->draw_gc, im, 0, 0, 0, 0,
             state->image_width, state->image_height);
  XDestroyImage (im);
  free (row);
}

static void
flip_images (m_state *state, Bool flipped_p)
{
  if (flipped_p != state->images_flipped_p)
    {
      state->images_flipped_p = flipped_p;
      flip_images_1 (state, 1);
      flip_images_1 (state, 2);
    }
}


/* When the subprocess has generated some output, this reads as much as it
   can into s->buf at s->buf_tail.
 */
static void
fill_input (m_state *s)
{
  int n = 0;
  int loadBytes;
  if(s->buf_done > s->buf_pos){
    loadBytes = (s->buf_done - s->buf_pos) - 1;
  }
  else{
    loadBytes = ((BUF_SIZE - s->buf_pos) + s->buf_done) - 1;
  }

  if (!s->tc)
    return;

  if (loadBytes > 0)
    {
      int c = textclient_getc (s->tc);
      n = (c > 0 ? 1 : -1);
      s->buf [s->buf_pos] = (char) c;
    }


  if (n > 0)
    {
        s->do_fill_buff = False;
      s->buf_pos = (s->buf_pos + n);
      if(s->buf_pos > BUF_SIZE){
        /* copy to start of buffer */
        /* areas shouldn't overlap, but just in case, use memmove */
        memmove(s->buf,s->buf+BUF_SIZE,s->buf_pos-BUF_SIZE);
      }
      s->buf_pos = s->buf_pos % BUF_SIZE;
    }
  else
    {
      /* Couldn't read anything from the buffer */
      /* Assume EOF has been reached, so start again */
      s->do_fill_buff = True;
    }
}


static void cursor_on_timer (XtPointer closure, XtIntervalId *id);
static void cursor_off_timer (XtPointer closure, XtIntervalId *id);

static Bool
set_cursor_1 (m_state *state, Bool on)
{
  Bool changed = (state->cursor_on != on);
  state->cursor_on = on;

  if (changed && state->cursor_x >= 0 && state->cursor_y >= 0)
    {
      m_cell *cell = &state->cells[state->grid_width * state->cursor_y
                                   + state->cursor_x];
      cell->glow = 0;
      cell->changed = True;
    }
  return changed;
}


static void
set_cursor (m_state *state, Bool on)
{
  if (set_cursor_1 (state, on))
    {
      if (state->cursor_timer)
        XtRemoveTimeOut (state->cursor_timer);
      state->cursor_timer = 0;
      if (on)
        cursor_on_timer (state, 0);
    }
}

static void
cursor_off_timer (XtPointer closure, XtIntervalId *id)
{
  m_state *state = (m_state *) closure;
  XtAppContext app = XtDisplayToApplicationContext (state->dpy);
  set_cursor_1 (state, False);
  state->cursor_timer = XtAppAddTimeOut (app, 333,
                                         cursor_on_timer, closure);
}

static void
cursor_on_timer (XtPointer closure, XtIntervalId *id)
{
  m_state *state = (m_state *) closure;
  XtAppContext app = XtDisplayToApplicationContext (state->dpy);
  set_cursor_1 (state, True);
  state->cursor_timer = XtAppAddTimeOut (app, 666,
                                         cursor_off_timer, closure);
}


static void
init_spinners (m_state *state)
{
  int i = state->nspinners;
  int x, y;
  m_cell *cell;

  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        cell = &state->cells[state->grid_width * y + x];
        cell->spinner = 0;
      }

  while (--i > 0)
    {
      x = random() % state->grid_width;
      y = random() % state->grid_height;
      cell = &state->cells[state->grid_width * y + x];
      cell->spinner = 1;
    }
}


static void
clear_spinners (m_state *state)
{
  int i;
  for (i = 0; i < state->grid_width * state->grid_height; i++)
    if (state->cells[i].spinner)
      {
        state->cells[i].spinner = 0;
        state->cells[i].changed = 1;
      }
}


static void set_mode (m_state *, m_mode);


static void
init_trace (m_state *state)
{
  char *s = get_string_resource (state->dpy, "tracePhone", "TracePhone");
  const char *s2;
  signed char *s3;
  if (!s)
    goto FAIL;

  state->tracing = (signed char *) malloc (strlen (s) + 1);
  s3 = state->tracing;

  for (s2 = s; *s2; s2++)
    if (*s2 >= '0' && *s2 <= '9')
      *s3++ = -*s2;
  *s3 = 0;

  if (s3 == state->tracing)
    goto FAIL;

  state->glyph_map = decimal_encoding;
  state->nglyphs = countof(decimal_encoding);

  return;

 FAIL:
  fprintf (stderr, "%s: bad phone number: \"%s\".\n",
           progname, s ? s : "(null)");

  if (s) free (s);
  if (state->tracing) free (state->tracing);
  state->tracing = 0;
  set_mode (state, MATRIX);
}


static void
init_drain (m_state *state)
{
  int i;

  set_cursor (state, False);
  state->cursor_x = -1;
  state->cursor_y = -1;

  /* Fill the top row with empty top-feeders, to clear the screen. */
  for (i = 0; i < state->grid_width; i++)
    {
      m_feeder *f = &state->feeders[i];
      f->y = -1;
      f->remaining = 0;
      f->throttle = 0;
    }

  /* Turn off all the spinners, else they never go away. */
  clear_spinners (state);
}

static Bool
screen_blank_p (m_state *state)
{
  int i;
  for (i = 0; i < state->grid_width * state->grid_height; i++)
    if (state->cells[i].glyph)
      return False;
  return True;
}


static void
set_mode (m_state *state, m_mode mode)
{
  if (mode == state->mode)
    return;

  state->mode = mode;
  state->typing = 0;

  switch (mode)
    {
    case MATRIX:
      state->glyph_map = matrix_encoding;
      state->nglyphs = countof(matrix_encoding);
      flip_images (state, True);
      init_spinners (state);
      break;
    case DNA:
      state->glyph_map = dna_encoding;
      state->nglyphs = countof(dna_encoding);
      flip_images (state, False);
      break;
    case BINARY:
      state->glyph_map = binary_encoding;
      state->nglyphs = countof(binary_encoding);
      flip_images (state, False);
      break;
    case HEX:
      state->glyph_map = hex_encoding;
      state->nglyphs = countof(hex_encoding);
      flip_images (state, False);
      break;
    case ASCII:
      state->glyph_map = ascii_encoding;
      state->nglyphs = countof(ascii_encoding);
      flip_images (state, False);
      break;
    case DEC:
    case TRACE_A:
    case TRACE_B:
    case NMAP:
    case KNOCK:
      state->glyph_map = decimal_encoding;
      state->nglyphs = countof(decimal_encoding);
      flip_images (state, False);
      break;
    case TRACE_TEXT_A: 
    case TRACE_TEXT_B:
      flip_images (state, False);
      init_trace (state);
      break;
    case DRAIN_TRACE_A:
    case DRAIN_TRACE_B:
    case DRAIN_KNOCK:
    case DRAIN_NMAP:
    case DRAIN_MATRIX:
      init_drain (state);
      break;
    case TRACE_DONE:
    case TRACE_FAIL:
      break;
    default:
      abort();
    }
}


static void *
xmatrix_init (Display *dpy, Window window)
{
  XGCValues gcv;
  char *insert, *mode;
  int i;
  m_state *state = (m_state *) calloc (sizeof(*state), 1);

  state->dpy = dpy;
  state->window = window;
  state->delay = get_integer_resource (dpy, "delay", "Integer");

  XGetWindowAttributes (dpy, window, &state->xgwa);

  state->small_p = (state->xgwa.width < 300);
  {
    const char *s = get_string_resource (dpy, "matrixFont", "String");
    if (!s || !*s || !strcasecmp(s, "large"))
      state->small_p = False;
    else if (!strcasecmp(s, "small"))
      state->small_p = True;
    else
      fprintf (stderr, "%s: matrixFont should be 'small' or 'large' not '%s'\n",
               progname, s);
  }

  load_images (dpy, state);

  gcv.foreground = get_pixel_resource(state->dpy, state->xgwa.colormap,
                                      "foreground", "Foreground");
  gcv.background = get_pixel_resource(state->dpy, state->xgwa.colormap,
                                      "background", "Background");
  state->draw_gc = XCreateGC (state->dpy, state->window,
                              GCForeground|GCBackground, &gcv);
  gcv.foreground = gcv.background;
  state->erase_gc = XCreateGC (state->dpy, state->window,
                               GCForeground|GCBackground, &gcv);

  state->scratch_gc = XCreateGC (state->dpy, state->window, 0, &gcv);

  /* Allocate colors for SYSTEM FAILURE box */
  {
    XColor boxcolors[] = {
      { 0, 0x0808, 0x1E1E, 0x0808, DoRed|DoGreen|DoBlue, 0 },
      { 0, 0x5A5A, 0xD2D2, 0x5A5A, DoRed|DoGreen|DoBlue, 0 },
      { 0, 0xE0E0, 0xF7F7, 0xE0E0, DoRed|DoGreen|DoBlue, 0 },
      { 0, 0x5A5A, 0xD2D2, 0x5A5A, DoRed|DoGreen|DoBlue, 0 },
      { 0, 0x0808, 0x1E1E, 0x0808, DoRed|DoGreen|DoBlue, 0 },
    };
  for (i = 0; i < countof(boxcolors); i++)
    {
      if (XAllocColor (state->dpy, state->xgwa.colormap, &boxcolors[i]))
        state->colors[i] = boxcolors[i].pixel;
      else
        state->colors[i] = gcv.foreground;  /* default black */
    }
  }

  state->char_width =  state->image_width  / CHAR_COLS;
  state->char_height = state->image_height / CHAR_ROWS;

  state->grid_width  = state->xgwa.width  / state->char_width;
  state->grid_height = state->xgwa.height / state->char_height;
  state->grid_width++;
  state->grid_height++;
  if (state->grid_width  < 5) state->grid_width  = 5;
  if (state->grid_height < 5) state->grid_height = 5;

  state->glyph_map = matrix_encoding;
  state->nglyphs = countof(matrix_encoding);

  state->cells = (m_cell *)
    calloc (sizeof(m_cell), state->grid_width * state->grid_height);
  state->background = (m_cell *)
    calloc (sizeof(m_cell), state->grid_width * state->grid_height);
  state->feeders = (m_feeder *) calloc (sizeof(m_feeder), state->grid_width);

  state->density = get_integer_resource (dpy, "density", "Integer");

  insert = get_string_resource(dpy, "insert", "Insert");
  if (insert && !strcmp(insert, "top"))
    {
      state->insert_top_p = True;
      state->insert_bottom_p = False;
    }
  else if (insert && !strcmp(insert, "bottom"))
    {
      state->insert_top_p = False;
      state->insert_bottom_p = True;
    }
  else if (insert && !strcmp(insert, "both"))
    {
      state->insert_top_p = True;
      state->insert_bottom_p = True;
    }
  else
    {
      if (insert && *insert)
        fprintf (stderr,
                 "%s: `insert' must be `top', `bottom', or `both', not `%s'\n",
                 progname, insert);
      state->insert_top_p = False;
      state->insert_bottom_p = True;
    }

  state->nspinners = get_integer_resource (dpy, "spinners", "Integer");

  if (insert)
    free (insert);

  state->knock_knock_p = get_boolean_resource (dpy, "knockKnock", "KnockKnock");

  state->use_pipe_p = get_boolean_resource (dpy, "usePipe", "Boolean");
  state->buf_pos = 1;
  state->buf[0] = ' '; /* spacer byte in buffer (space) */
  state->buf_done = 0;
  state->do_fill_buff = True;
  state->start_reveal_back_p = False;
  state->back_text_full_p = False;
  state->back_y = 0;
  state->back_pos = 0;

  state->mode = -1;
  state->def_mode = MATRIX;
  mode = get_string_resource (dpy, "mode", "Mode");
  if (mode && !strcasecmp(mode, "trace"))
    set_mode (state, ((random() % 3) ? TRACE_TEXT_A : TRACE_TEXT_B));
  else if (mode && !strcasecmp(mode, "crack"))
    set_mode (state, DRAIN_NMAP);
  else if (mode && !strcasecmp(mode, "dna")){
    set_mode (state, DNA);
    state->def_mode = DNA;
  }
  else if (mode && (!strcasecmp(mode, "bin") ||
                    !strcasecmp(mode, "binary"))){
    set_mode (state, BINARY);
    state->def_mode = BINARY;
  }
  else if (mode && (!strcasecmp(mode, "hex") ||
                    !strcasecmp(mode, "hexadecimal"))){
    set_mode (state, HEX);
    state->def_mode = HEX;
  }
  else if (mode && (!strcasecmp(mode, "dec") ||
                    !strcasecmp(mode, "decimal"))){
    set_mode (state, DEC);
    state->def_mode = DEC;
  }
  else if (mode && (!strcasecmp(mode, "asc") ||
                    !strcasecmp(mode, "ascii"))){
    set_mode (state, ASCII);
    state->def_mode = ASCII;
  }
  else if (mode && !strcasecmp(mode, "pipe"))
    {
      set_mode (state, ASCII);
      state->def_mode = ASCII;
      state->use_pipe_p = True;
      state->tc = textclient_open (dpy);
    }
  else if (!mode || !*mode || !strcasecmp(mode, "matrix"))
    set_mode (state, MATRIX);
  else
    {
      fprintf (stderr, "%s: `mode' must be ",progname);
      fprintf (stderr, "matrix, trace, dna, binary, ascii, hex, or pipe: ");
      fprintf (stderr, "not `%s'\n", mode);
      set_mode (state, MATRIX);
    }

  if (state->mode == MATRIX && get_boolean_resource (dpy, "trace", "Boolean"))
    set_mode (state, ((random() % 3) ? TRACE_TEXT_A : TRACE_TEXT_B));

  state->cursor_x = -1;
  state->cursor_y = -1;

  return state;
}


static void
insert_glyph (m_state *state, int glyph, int x, int y)
{
  Bool bottom_feeder_p = (y >= 0);
  m_cell *from, *to;
  if (y >= state->grid_height)
    return;

  if (bottom_feeder_p)
    {
      to = &state->cells[state->grid_width * y + x];
    }
  else
    {
      for (y = state->grid_height-1; y > 0; y--)
        {
          from = &state->cells[state->grid_width * (y-1) + x];
          to   = &state->cells[state->grid_width * y     + x];
          to->glyph   = from->glyph;
          to->glow    = from->glow;
          to->changed = 1;
        }
      to = &state->cells[x];
    }

  to->glyph = glyph;
  to->changed = 1;

  if (!to->glyph)
    ;
  else if (bottom_feeder_p)
    to->glow = 1 + (random() % (state->tracing ? 4 : 2));
  else
    to->glow = 0;
}


static void
place_back_char (m_state *state, char textc, int x, int y){
  if((x>=0) && (y>=0) &&
     (x < state->grid_width) && (y < state->grid_height)){
    m_cell *celltmp = &state->background[state->grid_width * y + x];
    celltmp -> glyph = char_map[(unsigned char)textc] + 1;
    if(!celltmp->glyph || (celltmp->glyph == 3)){
      celltmp -> glyph = char_map[32] + 1; 
    }
    celltmp -> changed = 1;
  } 
}

static void
place_back_text (m_state *state, char *text, int x, int y){
  int i;
  for(i=0; i<strlen(text); i++){
    place_back_char(state, text[i], x+i, y);
  }
}

static void
place_back_pipe (m_state *state, char textc){
  Bool new_line = False;
  /* gringer pipe insert */
  state->back_line[state->back_pos] = textc;
  if(textc == '\n'){
    state->back_line[state->back_pos] = '\0';
    new_line = True;
  }
  else if ((state->back_pos > (state->grid_width - 4)) || 
           (state->back_pos >= BUF_SIZE)){ /* off by 1? */
    state->back_line[++state->back_pos] = '\0';
    new_line = True;
  }
  else{
    state->back_pos++;
  }
  if(new_line){
    int startx = (state->grid_width >> 1) - 
      (strlen(state->back_line) >> 1);
    place_back_text(state, state->back_line, 
                    startx, state->back_y);
    state->back_pos = 0;
    state->back_y++;
    if(state->back_y >= (state->grid_height - 1)){
      state->back_y = 1;
      state->back_text_full_p = True;
      state->start_reveal_back_p = True;
    }
  }
}

static void
feed_matrix (m_state *state)
{
  int x;

  switch (state->mode)
    {
    case TRACE_A:
        {
          int L = strlen((char *) state->tracing);
          int count = 0;
          int i;

          for (i = 0; i < strlen((char *) state->tracing); i++)
            if (state->tracing[i] > 0) 
              count++;

          if (count >= L)
            {
              set_mode (state, TRACE_DONE);
              state->typing_delay = 1000000;
              return;
            }
          else
            {
              i = 5 + (30 / (count+1)); /* how fast numbers are discovered */

              if ((random() % i) == 0)
                {
                  i = random() % L;
                  if (state->tracing[i] < 0)
                    state->tracing[i] = -state->tracing[i];
                }
            }
        }
      break;

    case TRACE_B:
      if ((random() % 40) == 0)
        {
          set_mode (state, TRACE_FAIL);
          return;
        }
      break;

    case MATRIX: case DNA: case BINARY: case DEC: case HEX: case ASCII: 
    case DRAIN_TRACE_A:
    case DRAIN_TRACE_B:
    case DRAIN_KNOCK:
    case DRAIN_NMAP:
    case DRAIN_MATRIX:
      break;

    default:
      abort();
    }

  /*get input*/
  if((state->use_pipe_p) && (!state->back_text_full_p)){
    place_back_pipe(state, state->buf[state->buf_done]);
    state->buf_done = (state->buf_done + 1) % BUF_SIZE;
    if(state->buf_done == (state->buf_pos - 1)){
      state->do_fill_buff = True;
    }
    }
  if(state->buf_done == (state->buf_pos + 1)){
    state->do_fill_buff = False;
  }
  else{
    state->do_fill_buff = True;
    fill_input(state);
  }

  /* Update according to current feeders. */
  for (x = 0; x < state->grid_width; x++)
    {
      m_feeder *f = &state->feeders[x];

      if (f->throttle)		/* this is a delay tick, synced to frame. */
        {
          f->throttle--;
        }
      else if (f->remaining > 0)	/* how many items are in the pipe */
        {
          int g;
          long rval;
          if((state->use_pipe_p) && (!state->back_text_full_p)){
            rval = (int) state->buf[f->pipe_loc];
            if(++f->pipe_loc > (BUF_SIZE-1)){
              f->pipe_loc = 0;
              /*fill_input(state);*/
            }
            rval = (rval % state->nglyphs);
          }
          else{
            rval = (random() % state->nglyphs);
          }
          g = state->glyph_map[rval] + 1;
          insert_glyph (state, g, x, f->y);
          f->remaining--;
          if (f->y >= 0)  /* bottom_feeder_p */
            f->y++;
        }
      else				/* if pipe is empty, insert spaces */
        {
          insert_glyph (state, 0, x, f->y);
          if (f->y >= 0)  /* bottom_feeder_p */
            f->y++;
        }

      if ((random() % 10) == 0)		/* randomly change throttle speed */
        {
          f->throttle = ((random() % 5) + (random() % 5));
        }
    }
}


static void
redraw_cells (m_state *state, Bool active)
{
  int x, y;
  int count = 0;
  Bool use_back_p = False;

  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        m_cell *cell = &state->cells[state->grid_width * y + x];
        m_cell *back = &state->background[state->grid_width * y + x];
        Bool cursor_p = (state->cursor_on &&
                         x == state->cursor_x && 
                         y == state->cursor_y);

        if (cell->glyph)
          count++;
        else {
          if((state->start_reveal_back_p) && 
             (back->glyph) && !(state->mode == TRACE_A || 
                                state->mode == TRACE_B ||
                                state->mode == TRACE_DONE)){
            use_back_p = True;
            cell = back;
          }
        }

        /* In trace-mode, the state of each cell is random unless we have
           a match for this digit. */
        if (active && (state->mode == TRACE_A || 
                       state->mode == TRACE_B ||
                       state->mode == TRACE_DONE))
          {
            int xx = x % strlen((char *) state->tracing);
            Bool dead_p = state->tracing[xx] > 0;

            if (y == 0 && x == xx && !use_back_p)
              cell->glyph = (dead_p
                             ? state->glyph_map[state->tracing[xx]-'0'] + 1
                             : 0);
            else if (y == 0 && !use_back_p)
              cell->glyph = 0;
            else if (!use_back_p)
              cell->glyph = (dead_p ? 0 :
                             (state->glyph_map[(random()%state->nglyphs)]
                              + 1));
            if (!use_back_p)
            cell->changed = 1;
          }

        if (!cell->changed)
          continue;


        if (cell->glyph == 0 && !cursor_p && !use_back_p)
          XFillRectangle (state->dpy, state->window, state->erase_gc,
                          x * state->char_width,
                          y * state->char_height,
                          state->char_width,
                          state->char_height);
        else
          {
            int g = (cursor_p ? CURSOR_GLYPH : cell->glyph);
            int cx = (g - 1) % CHAR_COLS;
            int cy = (g - 1) / CHAR_COLS;
            int map = ((cell->glow != 0 || cell->spinner) ? GLOW_MAP :
                       PLAIN_MAP);

            XCopyArea (state->dpy, state->images[map],
                       state->window, state->draw_gc,
                       cx * state->char_width,
                       cy * state->char_height,
                       state->char_width,
                       state->char_height,
                       x * state->char_width,
                       y * state->char_height);
          }
        if (!use_back_p)
        cell->changed = 0;

        if (cell->glow > 0 && state->mode != NMAP && !use_back_p)
          {
            cell->glow--;
            cell->changed = 1;
          }
        else if (cell->glow < 0)
          abort();

        if (cell->spinner && active && !use_back_p)
          {
            cell->glyph = (state->glyph_map[(random()%state->nglyphs)] + 1);
            cell->changed = 1;
          }
      }
}


static int
densitizer (m_state *state)
{
  /* Horrid kludge that converts percentages (density of screen coverage)
     to the parameter that actually controls this.  I got this mapping
     empirically, on a 1024x768 screen.  Sue me. */
  if      (state->density < 10) return 85;
  else if (state->density < 15) return 60;
  else if (state->density < 20) return 45;
  else if (state->density < 25) return 25;
  else if (state->density < 30) return 20;
  else if (state->density < 35) return 15;
  else if (state->density < 45) return 10;
  else if (state->density < 50) return 8;
  else if (state->density < 55) return 7;
  else if (state->density < 65) return 5;
  else if (state->density < 80) return 3;
  else if (state->density < 90) return 2;
  else return 1;
}


static void
hack_text (m_state *state)
{
  if (!state->typing)
    {
      set_cursor (state, False);
      state->cursor_x = 0;
      state->cursor_y = 0;
      state->typing_scroll_p = False;
      state->typing_bold_p = False;
      state->typing_cursor_p = True;
      state->typing_stutter_p = False;
      state->typing_char_delay = 10000;
      state->typing_line_delay = 1500000;

      switch (state->mode)
        {
        case TRACE_TEXT_A:
        case TRACE_TEXT_B:
          clear_spinners (state);
          if (state->mode == TRACE_TEXT_A)
            {
              if (state->grid_width >= 52)
                state->typing =
                  ("Call trans opt: received. 2-19-98 13:24:18 REC:Log>\n"
                   "Trace program: running\n");
              else
                state->typing =
                  ("Call trans opt: received.\n2-19-98 13:24:18 REC:Log>\n"
                   "Trace program: running\n");
            }
          else
            {
              if (state->grid_width >= 52)
                state->typing =
                  ("Call trans opt: received. 9-18-99 14:32:21 REC:Log>\n"
                   "WARNING: carrier anomaly\n"
                   "Trace program: running\n");
              else
                state->typing =
                  ("Call trans opt: received.\n9-18-99 14:32:21 REC:Log>\n"
                   "WARNING: carrier anomaly\n"
                   "Trace program: running\n");
            }
          break;

        case TRACE_FAIL:
          {
            const char *s = "SYSTEM FAILURE\n";
            int i;
            float cx = (state->grid_width - strlen(s) - 1) / 2 - 0.5;
            float cy = (state->grid_height / 2) - 1.3;

            if (cy < 0) cy = 0;
            if (cx < 0) cx = 0;

            XFillRectangle (state->dpy, state->window, state->erase_gc,
                            cx * state->char_width,
                            cy * state->char_height,
                            strlen(s) * state->char_width,
                            state->char_height * 1.6);

            for (i = -2; i < 3; i++)
              {
                XGCValues gcv;
                gcv.foreground = state->colors[i + 2];
                XChangeGC (state->dpy, state->scratch_gc, GCForeground, &gcv);
                XDrawRectangle (state->dpy, state->window, state->scratch_gc,
                                cx * state->char_width - i,
                                cy * state->char_height - i,
                                strlen(s) * state->char_width + (2 * i),
                                (state->char_height * 1.6) + (2 * i));
              }

            /* If we don't clear these, part of the box may get overwritten */
            for (i = 0; i < state->grid_height * state->grid_width; i++)
              {
                m_cell *cell = &state->cells[i];
                cell->changed = 0;
              }

            state->cursor_x = (state->grid_width - strlen(s) - 1) / 2;
            state->cursor_y = (state->grid_height / 2) - 1;
            if (state->cursor_x < 0) state->cursor_x = 0;
            if (state->cursor_y < 0) state->cursor_y = 0;

            state->typing = s;
            state->typing_char_delay = 0;
            state->typing_cursor_p = False;
          }
          break;

        case KNOCK:
          {
            clear_spinners (state);
            state->typing = ("\001Wake up, Neo...\n"
                             "\001The Matrix has you...\n"
                             "\001Follow the white rabbit.\n"
                             "\n"
                             "Knock, knock, Neo.\n");

            state->cursor_x = 4;
            state->cursor_y = 2;
            state->typing_char_delay = 0;
            state->typing_line_delay = 2000000;
          }
          break;

        case NMAP:
          {
            /* Note that what Trinity is using here is moderately accurate:
               She runs nmap (http://www.insecure.org/nmap/) then breaks in
               with a (hypothetical) program called "sshnuke" that exploits
               the (very real) SSHv1 CRC32 compensation attack detector bug
               (http://staff.washington.edu/dittrich/misc/ssh-analysis.txt).

               The command syntax of the power grid control software looks a
               lot like Cisco IOS to me.  (IOS is a descendant of VMS.)
            */

            clear_spinners (state);
            state->typing =
# ifdef __GNUC__
            __extension__  /* don't warn about "string length is greater than
                              the length ISO C89 compilers are required to
                              support"... */

# endif
              ("# "
               "\010\010\010\010"
               "\001nmap -v -sS -O 10.2.2.2\n"
               "Starting nmap V. 2.54BETA25\n"
               "\010\010\010\010\010\010\010\010\010\010"
               "Insufficient responses for TCP sequencing (3), OS detection"
               " may be less accurate\n"
               "Interesting ports on 10.2.2.2:\n"
               "(The 1539 ports scanned but not shown below are in state:"
               " closed)\n"
               "Port       state       service\n"
               "22/tcp     open        ssh\n"
               "\n"
               "No exact OS matches for host\n"
               "\n"
               "Nmap run completed -- 1 IP address (1 host up) scanned\n"
               "# "

               "\010\010\010\010"
               "\001sshnuke 10.2.2.2 -rootpw=\"Z1ON0101\"\n"
               "Connecting to 10.2.2.2:ssh ... "
               "\010\010"
               "successful.\n"
               "Attempting to exploit SSHv1 CRC32 ... "
               "\010\010\010\010"
               "successful.\n"
               "Resetting root password to \"Z1ON0101\".\n"
               "\010\010"
               "System open: Access Level <9>\n"

               "# "
               "\010\010"

               "\001ssh 10.2.2.2 -l root\n"
               "\010\010"
               "root@10.2.2.2's password: "
               "\010\010\n"
               "\010\010\n"
               "RRF-CONTROL> "

               "\010\010"
               "\001disable grid nodes 21 - 48\n"
               "\n"
               "\002Warning: Disabling nodes 21-48 will disconnect sector 11"
               " (28 nodes)\n"
               "\n"
               "\002         ARE YOU SURE? (y/n) "

               "\010\010"
               "\001y\n"
               "\n"
               "\n"
               "\010\002Grid Node 21 offline...\n"
               "\010\002Grid Node 22 offline...\n"
               "\010\002Grid Node 23 offline...\n"
               "\010\002Grid Node 24 offline...\n"
               "\010\002Grid Node 25 offline...\n"
               "\010\002Grid Node 26 offline...\n"
               "\010\002Grid Node 27 offline...\n"
               "\010\002Grid Node 28 offline...\n"
               "\010\002Grid Node 29 offline...\n"
               "\010\002Grid Node 30 offline...\n"
               "\010\002Grid Node 31 offline...\n"
               "\010\002Grid Node 32 offline...\n"
               "\010\002Grid Node 33 offline...\n"
               "\010\002Grid Node 34 offline...\n"
               "\010\002Grid Node 35 offline...\n"
               "\010\002Grid Node 36 offline...\n"
               "\010\002Grid Node 37 offline...\n"
               "\010\002Grid Node 38 offline...\n"
               "\010\002Grid Node 39 offline...\n"
               "\010\002Grid Node 40 offline...\n"
               "\010\002Grid Node 41 offline...\n"
               "\010\002Grid Node 42 offline...\n"
               "\010\002Grid Node 43 offline...\n"
               "\010\002Grid Node 44 offline...\n"
               "\010\002Grid Node 45 offline...\n"
               "\010\002Grid Node 46 offline...\n"
               "\010\002Grid Node 47 offline...\n"
               "\010\002Grid Node 48 offline...\n"
               "\010\010"
               "\nRRF-CONTROL> "
               "\010\010\010\010\010\010\010\010"
               );

            state->cursor_x = 0;
            state->cursor_y = state->grid_height - 3;
            state->typing_scroll_p = True;
            state->typing_char_delay = 0;
            state->typing_line_delay = 20000;
          }
          break;

        default:
          abort();
          break;
        }

      state->typing_left_margin = state->cursor_x;
      state->typing_delay = state->typing_char_delay;
      if (state->typing_cursor_p)
        set_cursor (state, True);

# ifdef USE_IPHONE
  /* Stupid iPhone X bezel.
     #### This is the worst of all possible ways to do this!  But how else?
   */
  if (state->xgwa.width == 2436 || state->xgwa.height == 2436)
    switch (state->mode) 
      {
      case TRACE_TEXT_A:
      case TRACE_TEXT_B:
      case KNOCK:
      case NMAP:
        {
          int off = 5 * (state->small_p ? 2 : 1);
          if (state->xgwa.width > state->xgwa.height)
            {
              state->typing_left_margin += off;
              state->cursor_x += off;
            }
          else
            {
              state->cursor_y += off;
            }
        }
      default: break;
      }
# endif
    }
  else
    {
      Bool scrolled_p = False;
      unsigned char c, c1;
      int x = state->cursor_x;
      int y = state->cursor_y;

    AGAIN:
      c  = ((unsigned char *) state->typing)[0];
      c1 = c ? ((unsigned char *) state->typing)[1] : 0;

      state->typing_delay = (!c || c1 == '\n'
                             ? state->typing_line_delay
                             : state->typing_char_delay);
      if (! c)
        {
          state->typing_delay = 0;
          state->typing = 0;
          return;
        }

      if (state->typing_scroll_p &&
          (c == '\n' || 
           x >= state->grid_width - 1))
        {
          set_cursor (state, False);
          x = 0;
          y++;

          if (y >= state->grid_height-1)
            {
              int xx, yy;
              for (yy = 0; yy < state->grid_height-2; yy++)
                for (xx = 0; xx < state->grid_width; xx++)
                  {
                    int ii = yy     * state->grid_width + xx;
                    int jj = (yy+1) * state->grid_width + xx;
                    state->cells[ii] = state->cells[jj];
                    state->cells[ii].changed = 1;
                  }
              /* clear bottom row */
              for (xx = 0; xx < state->grid_width; xx++)
                {
                  int ii = yy * state->grid_width + xx;
                  state->cells[ii].glyph   = 0;
                  state->cells[ii].changed = 1;
                }
              y--;  /* move back up to bottom line */
              scrolled_p = True;
            }
        }

      if (c == '\n')
        {
          if (!state->typing_scroll_p)
            {
              int i, j;
              set_cursor (state, False);
              x = state->typing_left_margin;

              /* clear the line */
              i = state->grid_width * y;
              j = i + state->grid_width;
              for (; i < j; i++)
                {
                  state->cells[i].glyph   = 0;
                  state->cells[i].changed = 1;
                }
            }
          state->typing_bold_p = False;
          state->typing_stutter_p = False;
          scrolled_p = True;
        }

      else if (c == '\010')
        state->typing_delay += 500000;

      else if (c == '\001')
        {
          state->typing_stutter_p = True;
          state->typing_bold_p = False;
        }
      else if (c == '\002')
        state->typing_bold_p = True;

      else if (x < state->grid_width-1)
        {
          m_cell *cell = &state->cells[state->grid_width * y + x];
          cell->glyph = char_map[c] + 1;
          if (c == ' ' || c == '\t') cell->glyph = 0;
          cell->changed = 1;
          cell->glow = (state->typing_bold_p ? 127 : 0);
        }

      if (c >= ' ')
        x++;

      if (x >= state->grid_width-1)
        x = state->grid_width-1;

      state->typing++;

      if (state->typing_stutter_p)
        {
          if (state->typing_delay == 0)
            state->typing_delay = 20000;
          if (random() % 3)
            state->typing_delay += (0xFFFFFF & ((random() % 200000) + 1));
        }

      /* If there's no delay after this character, just keep going. */
      if (state->typing_delay == 0)
        goto AGAIN;

      if (scrolled_p || x != state->cursor_x || y != state->cursor_y)
        {
          set_cursor (state, False);
          state->cursor_x = x;
          state->cursor_y = y;
          if (state->typing_cursor_p)
            set_cursor (state, True);
        }
    }
}


static void
hack_matrix (m_state *state)
{
  int x;

  switch (state->mode)
    {
    case TRACE_DONE: case TRACE_FAIL:
      return;
    case TRACE_A: case TRACE_B:
    case MATRIX: case DNA: case BINARY: case DEC: case HEX: case ASCII:
      break;
    default:
      abort(); break;
    }

  /* Glow some characters. */
  if (!state->insert_bottom_p)
    {
      int i = random() % (state->grid_width / 2);
      while (--i > 0)
        {
          int yy = random() % state->grid_height;
          int xx = random() % state->grid_width;
          m_cell *cell = &state->cells[state->grid_width * yy + xx];
          if (cell->glyph && cell->glow == 0)
            {
              cell->glow = random() % 10;
              cell->changed = 1;
            }
        }
    }

  /* Change some of the feeders. */
  for (x = 0; x < state->grid_width; x++)
    {
      m_feeder *f = &state->feeders[x];
      Bool bottom_feeder_p;

      if (f->remaining > 0)	/* never change if pipe isn't empty */
        continue;

      if ((random() % densitizer(state)) != 0) /* then change N% of the time */
        continue;

      f->remaining = 3 + (random() % state->grid_height);
      f->throttle = ((random() % 5) + (random() % 5));

      if ((random() % 4) != 0)
        f->remaining = 0;

      if (state->mode == TRACE_A || state->mode == TRACE_B)
        bottom_feeder_p = True;
      else if (state->insert_top_p && state->insert_bottom_p)
        bottom_feeder_p = (random() & 1);
      else
        bottom_feeder_p = state->insert_bottom_p;

      if (bottom_feeder_p)
        f->y = random() % (state->grid_height / 2);
      else
        f->y = -1;
    }

  if (state->mode == MATRIX && (! (random() % 500)))
    init_spinners (state);
}


static unsigned long
xmatrix_draw (Display *dpy, Window window, void *closure)
{
  m_state *state = (m_state *) closure;

  if (state->typing_delay > 0)
    {
      state->typing_delay -= state->delay;
      if (state->typing_delay < 0)
        state->typing_delay = 0;
      redraw_cells (state, False);
      return state->delay;
    }

  switch (state->mode)
    {
    case MATRIX: case DNA: case BINARY: case DEC: case HEX: case ASCII:
    case TRACE_A: case TRACE_B:
      feed_matrix (state);
      hack_matrix (state);
      break;

    case DRAIN_TRACE_A:
    case DRAIN_TRACE_B:
    case DRAIN_KNOCK:
    case DRAIN_NMAP:
    case DRAIN_MATRIX:
      feed_matrix (state);
      if (screen_blank_p (state))
        {
          state->typing_delay = 500000;
          if(state->start_reveal_back_p){
            m_cell *back, *to;
            int x,y;
            state->typing_delay = 5000000;
            state->start_reveal_back_p = False;
            state->back_text_full_p = False;
            /* for loop to move background to foreground */
            for (y = 0; y < state->grid_height; y++){
              for (x = 0; x < state->grid_width; x++){
                to = &state->cells[state->grid_width * y + x];
                back = &state->background[state->grid_width * y + x];
                to->glyph = back->glyph;
                to->changed = back->changed;
                back->glyph = 0;
                back->changed = 0;
              }
            }
          }
          switch (state->mode)
            {
            case DRAIN_TRACE_A: set_mode (state, TRACE_TEXT_A); break;
            case DRAIN_TRACE_B: set_mode (state, TRACE_TEXT_B); break;
            case DRAIN_KNOCK:   set_mode (state, KNOCK);        break;
            case DRAIN_NMAP:    set_mode (state, NMAP);         break;
            case DRAIN_MATRIX:  set_mode (state, state->def_mode); break;
            default:            abort();                        break;
            }
        }
      break;

    case TRACE_DONE:
      set_mode (state, state->def_mode);
      break;

    case TRACE_TEXT_A: 
    case TRACE_TEXT_B: 
    case TRACE_FAIL: 
    case KNOCK: 
    case NMAP:
      hack_text (state);

      if (! state->typing)  /* done typing */
        {
          set_cursor (state, False);
          switch (state->mode)
            {
            case TRACE_TEXT_A: set_mode (state, TRACE_A); break;
            case TRACE_TEXT_B: set_mode (state, TRACE_B); break;
            case TRACE_FAIL:   set_mode (state, state->def_mode);  break;
            case KNOCK:        set_mode (state, state->def_mode);  break;
            case NMAP:         set_mode (state, state->def_mode);  break;
            default:           abort();                   break;
            }
        }
      break;

    default:
      abort();
    }
  if (state->start_reveal_back_p){
    set_mode (state, DRAIN_MATRIX);
  }
  if (state->mode == MATRIX &&
      state->knock_knock_p &&
      (! (random() % 10000)))
    {
      if (! (random() % 5))
        set_mode (state, DRAIN_NMAP);
      else
        set_mode (state, DRAIN_KNOCK);
    }

  redraw_cells (state, True);

#if 0
  {
    static int i = 0;
    static int ndens = 0;
    static int tdens = 0;
    i++;
    if (i > 50)
      {
        int dens = (100.0 *
                    (((double)count) /
                     ((double) (state->grid_width * state->grid_height))));
        tdens += dens;
        ndens++;
        printf ("density: %d%% (%d%%)\n", dens, (tdens / ndens));
        i = 0;
      }
  }
#endif

  return state->delay;
}


static void
xmatrix_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  m_state *state = (m_state *) closure;
  int ow = state->grid_width;
  int oh = state->grid_height;
  XGetWindowAttributes (state->dpy, state->window, &state->xgwa);
  state->grid_width  = state->xgwa.width  / state->char_width;
  state->grid_height = state->xgwa.height / state->char_height;
  state->grid_width++;
  state->grid_height++;
  if (state->grid_width  < 5) state->grid_width  = 5;
  if (state->grid_height < 5) state->grid_height = 5;

  if (ow != state->grid_width ||
      oh != state->grid_height)
    {
      m_cell *ncells = (m_cell *)
        calloc (sizeof(m_cell), state->grid_width * state->grid_height);
      m_cell *nbackground = (m_cell *)
        calloc (sizeof(m_cell), state->grid_width * state->grid_height);
      m_feeder *nfeeders = (m_feeder *)
        calloc (sizeof(m_feeder), state->grid_width);
      int x, y, i;

      /* fprintf(stderr, "resize: %d x %d  ==>  %d x %d\n",
         ow, oh, state->grid_width, state->grid_height); */

      for (y = 0; y < oh; y++)
        for (x = 0; x < ow; x++)
          if (x < ow && x < state->grid_width &&
              y < oh && y < state->grid_height){
            ncells[y * state->grid_width + x] =
              state->cells[y * ow + x];
            nbackground[y * state->grid_width + x] =
              state->background[y * ow + x];
          }
      free (state->cells);
      free (state->background);
      state->cells = ncells;
      state->background = nbackground;

      x = (ow < state->grid_width ? ow : state->grid_width);
      for (i = 0; i < x; i++)
        nfeeders[i] = state->feeders[i];
      free (state->feeders);
      state->feeders = nfeeders;
    }
  if (state->tc)
    textclient_reshape (state->tc,
                        state->xgwa.width,
                        state->xgwa.height,
                        state->grid_width  - 2,
                        state->grid_height - 1,
                        0);
}

static Bool
xmatrix_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  m_state *state = (m_state *) closure;

 if (event->xany.type == KeyPress)
   {
     KeySym keysym;
     char c = 0;
     XLookupString (&event->xkey, &c, 1, &keysym, 0);
     switch (c)
       {
       case '0':
         /*set_mode (state, DRAIN_MATRIX);*/
         state->back_y = 1;
         state->back_text_full_p = True;
         state->start_reveal_back_p = True;
         return True;

       case '+': case '=': case '>': case '.':
         state->density += 10;
         if (state->density > 100)
           state->density = 100;
         else
           return True;
         break;

       case '-': case '_': case '<': case ',':
         state->density -= 10;
         if (state->density < 0)
           state->density = 0;
         else
           return True;
         break;

       case '[': case '(': case '{':
         state->insert_top_p    = True;
         state->insert_bottom_p = False;
         return True;

       case ']': case ')': case '}':
         state->insert_top_p    = False;
         state->insert_bottom_p = True;
         return True;

       case '\\': case '|':
         state->insert_top_p    = True;
         state->insert_bottom_p = True;
         return True;

       case 't':
         set_mode (state, DRAIN_TRACE_A);
         return True;

       case 'T':
         set_mode (state, DRAIN_TRACE_B);
         return True;

       case 'k':
         set_mode (state, DRAIN_KNOCK);
         return True;

       case 'c':
         set_mode (state, DRAIN_NMAP);
         return True;

       default:
         break;
       }
   }

 if (screenhack_event_helper (dpy, window, event))
   {
     set_mode (state, DRAIN_MATRIX);
     return True;
   }

  return False;
}

static void
xmatrix_free (Display *dpy, Window window, void *closure)
{
  m_state *state = (m_state *) closure;
  if (state->tc)
    textclient_close (state->tc);
  if (state->cursor_timer)
    XtRemoveTimeOut (state->cursor_timer);

  /* #### there's more to free here */

  free (state);
}

static const char *xmatrix_defaults [] = {
  ".background:		   black",
  ".foreground:		   #00AA00",
  ".lowrez:		   true",  /* Small font is unreadable at 5120x2880 */
  "*fpsSolid:		   true",
  "*matrixFont:		   large",
  "*delay:		   10000",
  "*insert:		   both",
  "*mode:		   Matrix",
  "*tracePhone:            (312) 555-0690",
  "*spinners:		   5",
  "*density:		   75",
  "*trace:		   True",
  "*knockKnock:		   True",
  "*usePipe:		   False",
  "*usePty:                False",
  "*program:		   xscreensaver-text --latin1",
  "*geometry:		   960x720",
  0
};

static XrmOptionDescRec xmatrix_options [] = {
  { "-small",		".matrixFont",		XrmoptionNoArg, "Small" },
  { "-large",		".matrixFont",		XrmoptionNoArg, "Large" },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-insert",		".insert",		XrmoptionSepArg, 0 },
  { "-top",		".insert",		XrmoptionNoArg, "top" },
  { "-bottom",		".insert",		XrmoptionNoArg, "bottom" },
  { "-both",		".insert",		XrmoptionNoArg, "both" },
  { "-density",		".density",		XrmoptionSepArg, 0 },
  { "-trace",		".trace",		XrmoptionNoArg, "True" },
  { "-no-trace",	".trace",		XrmoptionNoArg, "False" },
  { "-crack",		".mode",		XrmoptionNoArg, "crack"},
  { "-phone",		".tracePhone",		XrmoptionSepArg, 0 },
  { "-mode",		".mode",		XrmoptionSepArg, 0 },
  { "-dna",		".mode",		XrmoptionNoArg, "DNA" },
  { "-binary",		".mode",		XrmoptionNoArg, "binary" },
  { "-hexadecimal",	".mode",		XrmoptionNoArg, "hexadecimal"},
  { "-decimal",		".mode",		XrmoptionNoArg, "decimal"},
  { "-knock-knock",	".knockKnock",		XrmoptionNoArg, "True" },
  { "-no-knock-knock",	".knockKnock",		XrmoptionNoArg, "False" },
  { "-ascii",		".mode",		XrmoptionNoArg, "ascii"},
  { "-pipe",	        ".usePipe",		XrmoptionNoArg, "True" },
  { "-no-pipe",	        ".usePipe",		XrmoptionNoArg, "False" },
  { "-program",	        ".program",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("XMatrix", xmatrix)
