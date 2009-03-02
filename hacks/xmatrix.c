/* xscreensaver, Copyright (c) 1999, 2001 Jamie Zawinski <jwz@jwz.org>
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
 * Windows/Mac version at http://www.whatisthematrix.com/) doesn't match my
 * memory of what the screens in the movie looked like, so my `xmatrix'
 * does things differently.
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
 *     ==========================================================
 *
 */

#include "screenhack.h"
#include "xpm-pixmap.h"
#include <stdio.h>
#include <X11/Xutil.h>

#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
# include "images/matrix0.xpm"
# include "images/matrix1.xpm"
# include "images/matrix2.xpm"
# include "images/matrix0b.xpm"
# include "images/matrix1b.xpm"
# include "images/matrix2b.xpm"
#endif

#include "images/matrix0.xbm"
#include "images/matrix1.xbm"
#include "images/matrix2.xbm"
#include "images/matrix0b.xbm"
#include "images/matrix1b.xbm"
#include "images/matrix2b.xbm"

#define CHAR_COLS 16
#define CHAR_ROWS 13
#define CHAR_MAPS 3
#define FADE_MAP  0
#define PLAIN_MAP 1
#define GLOW_MAP  2

typedef struct {
           int glow    : 8;
  unsigned int glyph   : 9;  /* note: 9 bit characters! */
  unsigned int changed : 1;
  unsigned int spinner : 1;
} m_cell;

typedef struct {
  int remaining;
  int throttle;
  int y;
} m_feeder;

#define countof(x) (sizeof(x)/sizeof(*(x)))

static int matrix_encoding[] = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                 192, 193, 194, 195, 196, 197, 198, 199,
                                 200, 201, 202, 203, 204, 205, 206, 207 };
static int decimal_encoding[]  = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 };
static int hex_encoding[]      = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                   33, 34, 35, 36, 37, 38 };
static int binary_encoding[] = { 16, 17 };
static int dna_encoding[]    = { 33, 35, 39, 52 };
static unsigned char char_map[256] = {
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

typedef enum { TRACE0, TRACE1, TRACE2,
               KNOCK0, KNOCK1, KNOCK2, KNOCK3,
               KNOCK4, KNOCK5, KNOCK6, KNOCK7,
               MATRIX, DNA, BINARY, HEX } m_mode;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  GC draw_gc, erase_gc;
  int grid_width, grid_height;
  int char_width, char_height;
  m_cell *cells;
  m_feeder *feeders;
  int nspinners;
  Bool small_p;
  Bool insert_top_p, insert_bottom_p;
  m_mode mode;
  signed char *tracing;
  int density;

  Pixmap images[CHAR_MAPS];
  int image_width, image_height;

  int nglyphs;
  int *glyph_map;

} m_state;


static void
load_images_1 (m_state *state, int which)
{
#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
  if (!get_boolean_resource ("mono", "Boolean") &&
      state->xgwa.depth > 1)
    {
      char **bits =
        (which == 0 ? (state->small_p ? matrix0b_xpm : matrix0_xpm) :
         which == 1 ? (state->small_p ? matrix1b_xpm : matrix1_xpm) :
         (state->small_p ? matrix2b_xpm : matrix2_xpm));

      state->images[which] =
        xpm_data_to_pixmap (state->dpy, state->window, bits,
                            &state->image_width, &state->image_height, 0);
    }
  else
#endif /* !HAVE_XPM && !HAVE_GDK_PIXBUF */
    {
      unsigned long fg, bg;
      state->image_width  = (state->small_p ? matrix0b_width :matrix0_width);
      state->image_height = (state->small_p ? matrix0b_height:matrix0_height);
      fg = get_pixel_resource("foreground", "Foreground",
                              state->dpy, state->xgwa.colormap);
      bg = get_pixel_resource("background", "Background",
                              state->dpy, state->xgwa.colormap);
      state->images[which] =
        XCreatePixmapFromBitmapData (state->dpy, state->window, (char *)
                (which == 0 ? (state->small_p ? matrix0b_bits :matrix0_bits) :
                 which == 1 ? (state->small_p ? matrix1b_bits :matrix1_bits) :
                              (state->small_p ? matrix2b_bits :matrix2_bits)),
                                     state->image_width, state->image_height,
                                     bg, fg, state->xgwa.depth);
    }
}


static void
load_images (m_state *state)
{
  load_images_1 (state, 0);
  load_images_1 (state, 1);
  load_images_1 (state, 2);
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
flip_images (m_state *state)
{
  flip_images_1 (state, 0);
  flip_images_1 (state, 1);
  flip_images_1 (state, 2);
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
init_trace (m_state *state)
{
  char *s = get_string_resource ("tracePhone", "TracePhone");
  char *s2, *s3;
  int i;
  if (!s)
    goto FAIL;

  state->tracing = (char *) malloc (strlen (s) + 1);
  s3 = state->tracing;

  for (s2 = s; *s2; s2++)
    if (*s2 >= '0' && *s2 <= '9')
      *s3++ = *s2;
  *s3 = 0;

  if (s3 == (char *) state->tracing)
    goto FAIL;

  for (i = 0; i < strlen(state->tracing); i++)
    state->tracing[i] = -state->tracing[i];

  state->glyph_map = decimal_encoding;
  state->nglyphs = countof(decimal_encoding);

  return;

 FAIL:
  fprintf (stderr, "%s: bad phone number: \"%s\".\n",
           progname, s ? s : "(null)");

  if (s) free (s);
  if (state->tracing) free (state->tracing);
  state->tracing = 0;
  state->mode = MATRIX;
}


static m_state *
init_matrix (Display *dpy, Window window)
{
  XGCValues gcv;
  char *insert, *mode;
  m_state *state = (m_state *) calloc (sizeof(*state), 1);

  state->dpy = dpy;
  state->window = window;

  XGetWindowAttributes (dpy, window, &state->xgwa);

  {
    const char *s = get_string_resource ("small", "Boolean");
    if (s && *s)
      state->small_p = get_boolean_resource ("small", "Boolean");
    else
      state->small_p = (state->xgwa.width < 300);
  }

  load_images (state);

  gcv.foreground = get_pixel_resource("foreground", "Foreground",
                                      state->dpy, state->xgwa.colormap);
  gcv.background = get_pixel_resource("background", "Background",
                                      state->dpy, state->xgwa.colormap);
  state->draw_gc = XCreateGC (state->dpy, state->window,
                              GCForeground|GCBackground, &gcv);
  gcv.foreground = gcv.background;
  state->erase_gc = XCreateGC (state->dpy, state->window,
                               GCForeground|GCBackground, &gcv);

  state->char_width =  state->image_width  / CHAR_COLS;
  state->char_height = state->image_height / CHAR_ROWS;

  state->grid_width  = state->xgwa.width  / state->char_width;
  state->grid_height = state->xgwa.height / state->char_height;
  state->grid_width++;
  state->grid_height++;

  state->glyph_map = matrix_encoding;
  state->nglyphs = countof(matrix_encoding);

  state->cells = (m_cell *)
    calloc (sizeof(m_cell), state->grid_width * state->grid_height);
  state->feeders = (m_feeder *) calloc (sizeof(m_feeder), state->grid_width);

  state->density = get_integer_resource ("density", "Integer");

  insert = get_string_resource("insert", "Insert");
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

  state->nspinners = get_integer_resource ("spinners", "Integer");

  if (insert)
    free (insert);

  mode = get_string_resource ("mode", "Mode");
  if (mode && !strcasecmp(mode, "trace"))
    state->mode = TRACE0;
  else if (mode && !strcasecmp(mode, "dna"))
    state->mode = DNA;
  else if (mode && !strcasecmp(mode, "binary"))
    state->mode = BINARY;
  else if (mode && (!strcasecmp(mode, "hex") ||
                    !strcasecmp(mode, "hexadecimal")))
    state->mode = HEX;
  else if (!mode || !*mode || !strcasecmp(mode, "matrix"))
    state->mode = MATRIX;
  else
    {
      fprintf (stderr,
           "%s: `mode' must be matrix, trace, dna, binary, or hex: not `%s'\n",
               progname, mode);
      state->mode = MATRIX;
    }

  if (state->mode == DNA)
    {
      state->glyph_map = dna_encoding;
      state->nglyphs = countof(dna_encoding);
    }
  else if (state->mode == BINARY)
    {
      state->glyph_map = binary_encoding;
      state->nglyphs = countof(binary_encoding);
    }
  else if (state->mode == HEX)
    {
      state->glyph_map = hex_encoding;
      state->nglyphs = countof(hex_encoding);
    }
  else if (state->mode == HEX)
    {
      state->glyph_map = hex_encoding;
      state->nglyphs = countof(hex_encoding);
    }
  else if (state->mode == TRACE0)
    init_trace (state);
  else
    {
      flip_images (state);
      init_spinners (state);
    }

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
    to->glow = 1 + (random() % 2);
  else
    to->glow = 0;
}


static void
feed_matrix (m_state *state)
{
  int x;

  switch (state->mode)
    {
    case TRACE2: case MATRIX: case DNA: case BINARY: case HEX:
      break;
    default:
      return;
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
          int g = state->glyph_map[(random() % state->nglyphs)] + 1;
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
  int i;
  int x = 0;
  const char *s;
  switch (state->mode)
    {
    case TRACE0: s = "Call trans opt: received.\n"
                     "2-19-98 13:24:18 REC:Log>_"; break;
    case TRACE1: s = "Trace program: running_"; break;

    case KNOCK0: s = "Wake up, Neo..."; break;
    case KNOCK1: s = ""; break;
    case KNOCK2: s = "The Matrix has you..."; break;
    case KNOCK3: s = ""; break;
    case KNOCK4: s = "Follow the white rabbit..."; break;
    case KNOCK5: s = ""; break;
    case KNOCK6: s = "Knock knock, Neo."; break;
    case KNOCK7: s = ""; break;

    default: abort(); break;
    }

  for (i = 0; i < state->grid_height * state->grid_width; i++)
    {
      m_cell *cell = &state->cells[i];
      cell->changed = (cell->glyph != 0);
      cell->glyph = 0;
    }

  if (state->mode == TRACE0 || state->mode == TRACE1)
    i = 0;
  else
    {
      int y;
      x = ((int)state->grid_width - (int)strlen(s)) / 2;
      y = (state->grid_height / 2) - 1;
      if (y < 0) y = 0;
      if (x < 0) x = 0;
      i = (y * state->grid_width) + x;
    }

  while (*s)
    {
      if (*s == '\n')
        {
          i = ((i / state->grid_width) + 1) * state->grid_width;
          x = 0;
        }
      else
        {
          m_cell *cell = &state->cells[i];
          if (x < state->grid_width-1)
            {
              cell->glyph = char_map[(unsigned char) *s] + 1;
              if (*s == ' ' || *s == '\t') cell->glyph = 0;
              cell->changed = 1;
              i++;
            }
          x++;
        }
      s++;
    }
}

static void
roll_state (m_state *state)
{
  int delay = 0;
  switch (state->mode)
    {
    case TRACE0:
      delay = 3;
      state->mode = TRACE1;
      break;

    case TRACE1:
      delay = 2;
      state->mode = TRACE2;
      break;

    case TRACE2:
      {
        Bool any = False;
        int i;
        for (i = 0; i < strlen(state->tracing); i++)
          if (state->tracing[i] < 0) any = True;

        if (!any)
          {
            XSync (state->dpy, False);
            sleep (3);
            state->mode = MATRIX;
            state->glyph_map = matrix_encoding;
            state->nglyphs = countof(matrix_encoding);
            flip_images (state);
            free (state->tracing);
            state->tracing = 0;
          }
        else if ((random() % 10) == 0)
          {
            int x = random() % strlen(state->tracing);
            if (state->tracing[x] < 0)
              state->tracing[x] = -state->tracing[x];
          }
        break;
      }

    case KNOCK0: delay = 1; state->mode++; break; /* wake */
    case KNOCK1: delay = 4; state->mode++; break;
    case KNOCK2: delay = 2; state->mode++; break; /* has */
    case KNOCK3: delay = 4; state->mode++; break;
    case KNOCK4: delay = 2; state->mode++; break; /* rabbit */
    case KNOCK5: delay = 4; state->mode++; break;
    case KNOCK6: delay = 4; state->mode++; break; /* knock */
    case KNOCK7:
      state->mode = MATRIX;
      state->glyph_map = matrix_encoding;
      state->nglyphs = countof(matrix_encoding);
      flip_images (state);
      break;

    case MATRIX:
      if (! (random() % 5000))
        {
          state->mode = KNOCK0;
          flip_images (state);
        }
      break;

    case DNA: case BINARY: case HEX:
      break;

    default:
      abort();
      break;
    }

  if (delay)
    {
      XSync (state->dpy, False);
      sleep (delay);
    }
}


static void
hack_matrix (m_state *state)
{
  int x;

  switch (state->mode)
    {
    case TRACE0: case TRACE1:
    case KNOCK0: case KNOCK1: case KNOCK2: case KNOCK3:
    case KNOCK4: case KNOCK5: case KNOCK6: case KNOCK7:
      hack_text (state);
      return;
    case TRACE2: case MATRIX: case DNA: case BINARY: case HEX:
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
          int x = random() % state->grid_width;
          int y = random() % state->grid_height;
          m_cell *cell = &state->cells[state->grid_width * y + x];
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

      if (state->mode == TRACE2)
        bottom_feeder_p = True;
      if (state->insert_top_p && state->insert_bottom_p)
        bottom_feeder_p = (random() & 1);
      else
        bottom_feeder_p = state->insert_bottom_p;

      if (bottom_feeder_p)
        f->y = random() % (state->grid_height / 2);
      else
        f->y = -1;
    }

  if (!state->mode == TRACE2 &&
      ! (random() % 500))
    init_spinners (state);
}


static void
draw_matrix (m_state *state)
{
  int x, y;
  int count = 0;

  feed_matrix (state);
  hack_matrix (state);

  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        m_cell *cell = &state->cells[state->grid_width * y + x];

        if (cell->glyph)
          count++;

        if (state->mode == TRACE2)
          {
            int xx = x % strlen(state->tracing);
            Bool dead_p = state->tracing[xx] > 0;

            if (y == 0 && x == xx)
              cell->glyph = (dead_p
                             ? state->glyph_map[state->tracing[xx]-'0'] + 1
                             : 0);
            else if (y == 0)
              cell->glyph = 0;
            else
              cell->glyph = (dead_p ? 0 :
                             (state->glyph_map[(random()%state->nglyphs)]
                              + 1));

            cell->changed = 1;
          }

        if (!cell->changed)
          continue;

        if (cell->glyph == 0)
          XFillRectangle (state->dpy, state->window, state->erase_gc,
                          x * state->char_width,
                          y * state->char_height,
                          state->char_width,
                          state->char_height);
        else
          {
            int cx = (cell->glyph - 1) % CHAR_COLS;
            int cy = (cell->glyph - 1) / CHAR_COLS;
            int map = ((cell->glow > 0 || cell->spinner) ? GLOW_MAP :
                       (cell->glow == 0) ? PLAIN_MAP :
                       GLOW_MAP);

            XCopyArea (state->dpy, state->images[map],
                       state->window, state->draw_gc,
                       cx * state->char_width,
                       cy * state->char_height,
                       state->char_width,
                       state->char_height,
                       x * state->char_width,
                       y * state->char_height);
          }

        cell->changed = 0;

        if (cell->glow > 0)
          {
            cell->glow--;
            cell->changed = 1;
          }
        else if (cell->glow < 0)
          {
            cell->glow++;
            if (cell->glow == 0)
              cell->glyph = 0;
            cell->changed = 1;
          }

        if (cell->spinner)
          {
            cell->glyph = (state->glyph_map[(random()%state->nglyphs)] + 1);
            cell->changed = 1;
          }
      }

  roll_state (state);

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

}


char *progclass = "XMatrix";

char *defaults [] = {
  ".background:		   black",
  ".foreground:		   green",
  "*small:		   ",
  "*delay:		   10000",
  "*insert:		   both",
  "*mode:		   Matrix",
  "*tracePhone:            (212) 555-0690",
  "*spinners:		   5",
  "*density:		   75",
  0
};

XrmOptionDescRec options [] = {
  { "-small",		".small",		XrmoptionNoArg, "True" },
  { "-large",		".small",		XrmoptionNoArg, "False" },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-top",		".insert",		XrmoptionNoArg, "top" },
  { "-bottom",		".insert",		XrmoptionNoArg, "bottom" },
  { "-both",		".insert",		XrmoptionNoArg, "both" },
  { "-density",		".density",		XrmoptionSepArg, 0 },
  { "-trace",		".mode",		XrmoptionNoArg, "trace" },
  { "-phone",		".tracePhone",		XrmoptionSepArg, 0 },
  { "-dna",		".mode",		XrmoptionNoArg, "DNA" },
  { "-binary",		".mode",		XrmoptionNoArg, "binary" },
  { "-hexadecimal",	".mode",		XrmoptionNoArg, "hexadecimal"},
  { 0, 0, 0, 0 }
};


void
screenhack (Display *dpy, Window window)
{
  m_state *state = init_matrix (dpy, window);
  int delay = get_integer_resource ("delay", "Integer");
  while (1)
    {
      draw_matrix (state);
      XSync (dpy, False);
      screenhack_handle_events (dpy);
      if (delay) usleep (delay);
    }
}
