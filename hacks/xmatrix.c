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
 *         the title sequences.
 *
 *     ==========================================================
 *
 */

#include "screenhack.h"
#include "xpm-pixmap.h"
#include <stdio.h>
#include <X11/Xutil.h>

#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
# include "images/matrix1.xpm"
# include "images/matrix2.xpm"
# include "images/matrix1b.xpm"
# include "images/matrix2b.xpm"
#endif

#include "images/matrix1.xbm"
#include "images/matrix2.xbm"
#include "images/matrix1b.xbm"
#include "images/matrix2b.xbm"

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

#define CURSOR_GLYPH 97

typedef enum { TRACEA1, TRACEA2,
               TRACEB1, TRACEB2, SYSTEMFAILURE,
               KNOCK, NMAP, MATRIX, DNA, BINARY, DEC, HEX } m_mode;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  GC draw_gc, erase_gc, scratch_gc;
  int grid_width, grid_height;
  int char_width, char_height;
  m_cell *cells;
  m_cell *cursor;
  m_feeder *feeders;
  int nspinners;
  Bool knock_knock_p;
  Bool small_p;
  Bool insert_top_p, insert_bottom_p;
  m_mode mode;
  signed char *tracing;
  int density;

  Pixmap images[CHAR_MAPS];
  int image_width, image_height;

  int nglyphs;
  int *glyph_map;

  unsigned long colors[5];
} m_state;


static void
load_images_1 (m_state *state, int which)
{
#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
  if (!get_boolean_resource ("mono", "Boolean") &&
      state->xgwa.depth > 1)
    {
      char **bits =
        (which == 1 ? (state->small_p ? matrix1b_xpm : matrix1_xpm) :
         (state->small_p ? matrix2b_xpm : matrix2_xpm));

      state->images[which] =
        xpm_data_to_pixmap (state->dpy, state->window, bits,
                            &state->image_width, &state->image_height, 0);
    }
  else
#endif /* !HAVE_XPM && !HAVE_GDK_PIXBUF */
    {
      unsigned long fg, bg;
      state->image_width  = (state->small_p ? matrix1b_width :matrix1_width);
      state->image_height = (state->small_p ? matrix1b_height:matrix1_height);
      fg = get_pixel_resource("foreground", "Foreground",
                              state->dpy, state->xgwa.colormap);
      bg = get_pixel_resource("background", "Background",
                              state->dpy, state->xgwa.colormap);
      state->images[which] =
        XCreatePixmapFromBitmapData (state->dpy, state->window, (char *)
                (which == 1 ? (state->small_p ? matrix1b_bits :matrix1_bits) :
                              (state->small_p ? matrix2b_bits :matrix2_bits)),
                                     state->image_width, state->image_height,
                                     bg, fg, state->xgwa.depth);
    }
}


static void
load_images (m_state *state)
{
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
  int i;
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
  state->cursor = NULL;
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

  state->knock_knock_p = get_boolean_resource ("knockKnock", "KnockKnock");

  mode = get_string_resource ("mode", "Mode");
  if (mode && !strcasecmp(mode, "trace"))
    state->mode = (((random() % 3) == 0) ? TRACEB1 : TRACEA1);
  else if (mode && !strcasecmp(mode, "crack"))
    state->mode = NMAP;
  else if (mode && !strcasecmp(mode, "dna"))
    state->mode = DNA;
  else if (mode && (!strcasecmp(mode, "bin") ||
                    !strcasecmp(mode, "binary")))
    state->mode = BINARY;
  else if (mode && (!strcasecmp(mode, "hex") ||
                    !strcasecmp(mode, "hexadecimal")))
    state->mode = HEX;
  else if (mode && (!strcasecmp(mode, "dec") ||
                    !strcasecmp(mode, "decimal")))
    state->mode = DEC;
  else if (!mode || !*mode || !strcasecmp(mode, "matrix"))
    state->mode = MATRIX;
  else
    {
      fprintf (stderr,
           "%s: `mode' must be matrix, trace, dna, binary, or hex: not `%s'\n",
               progname, mode);
      state->mode = MATRIX;
    }

  switch (state->mode)
    {
    case DNA:
      state->glyph_map = dna_encoding;
      state->nglyphs = countof(dna_encoding);
      break;
    case BINARY:
      state->glyph_map = binary_encoding;
      state->nglyphs = countof(binary_encoding);
      break;
    case DEC:
      state->glyph_map = decimal_encoding;
      state->nglyphs = countof(decimal_encoding);
      break;
    case HEX:
      state->glyph_map = hex_encoding;
      state->nglyphs = countof(hex_encoding);
      break;
    case TRACEA1: case TRACEB1:
      init_trace (state);
      break;
    case NMAP:
      break;
    case MATRIX:
      flip_images (state);
      init_spinners (state);
      break;
    default:
      abort();
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
    to->glow = 1 + (random() % (state->tracing ? 4 : 2));
  else
    to->glow = 0;
}


static void
feed_matrix (m_state *state)
{
  int x;

  switch (state->mode)
    {
    case TRACEA2: case TRACEB2:
    case MATRIX: case DNA: case BINARY: case DEC: case HEX:
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


static void
redraw_cells (m_state *state, Bool active)
{
  int x, y;
  int count = 0;

  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        m_cell *cell = &state->cells[state->grid_width * y + x];

        if (cell->glyph)
          count++;

        if ((state->mode == TRACEA2 || state->mode == TRACEB2) && active)
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

        if (cell->glyph == 0 && cell != state->cursor)
          XFillRectangle (state->dpy, state->window, state->erase_gc,
                          x * state->char_width,
                          y * state->char_height,
                          state->char_width,
                          state->char_height);
        else
          {
            int g = (cell == state->cursor ? CURSOR_GLYPH : cell->glyph);
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

        cell->changed = 0;

        if (cell->glow > 0)
          {
            cell->glow--;
            cell->changed = 1;
          }
        else if (cell->glow < 0)
          abort();

        if (cell->spinner && active)
          {
            cell->glyph = (state->glyph_map[(random()%state->nglyphs)] + 1);
            cell->changed = 1;
          }
      }

  if (state->cursor)
    {
      state->cursor->changed = 1;
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


static void drain_matrix (m_state *);

static void
handle_events (m_state *state)
{
  XSync (state->dpy, False);
  while (XPending (state->dpy))
    {
      XEvent event;
      XNextEvent (state->dpy, &event);

      if (event.xany.type == ConfigureNotify)
        {
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
                calloc (sizeof(m_cell),
                        state->grid_width * state->grid_height);
              m_feeder *nfeeders = (m_feeder *)
                calloc (sizeof(m_feeder), state->grid_width);
              int x, y, i;

              /* fprintf(stderr, "resize: %d x %d  ==>  %d x %d\n",
                        ow, oh, state->grid_width, state->grid_height); */

              for (y = 0; y < oh; y++)
                for (x = 0; x < ow; x++)
                  if (x < ow && x < state->grid_width &&
                      y < oh && y < state->grid_height)
                    ncells[y * state->grid_width + x] =
                      state->cells[y * ow + x];
              free (state->cells);
              state->cells = ncells;

              x = (ow < state->grid_width ? ow : state->grid_width);
              for (i = 0; i < x; i++)
                nfeeders[i] = state->feeders[i];
              free (state->feeders);
              state->feeders = nfeeders;
            }
        }
      else if (event.xany.type == KeyPress)
        {
          KeySym keysym;
          char c = 0;
          XLookupString (&event.xkey, &c, 1, &keysym, 0);
          if (c == '0' && !state->tracing)
            {
              drain_matrix (state);
              return;
            }
          else if (c == '+' || c == '=' || c == '>' || c == '.')
            {
              state->density += 10;
              if (state->density > 100)
                state->density = 100;
              else
                return;
            }
          else if (c == '-' || c == '_' || c == '<' || c == ',')
            {
              state->density -= 10;
              if (state->density < 0)
                state->density = 0;
              else
                return;
            }
          else if (c == '[' || c == '(' || c == '{')
            {
              state->insert_top_p    = True;
              state->insert_bottom_p = False;
              return;
            }
          else if (c == ']' || c == ')' || c == '}')
            {
              state->insert_top_p    = False;
              state->insert_bottom_p = True;
              return;
            }
          else if (c == '\\' || c == '|')
            {
              state->insert_top_p    = True;
              state->insert_bottom_p = True;
              return;
            }
          else if ((c == 't' || c == 'T') && state->mode == MATRIX)
            {
              state->mode = (c == 't' ? TRACEA1 : TRACEB1);
              flip_images (state);
              init_trace (state);
              return;
            }
          else if ((c == 'c' || c == 'k') && state->mode == MATRIX)
            {
              drain_matrix (state);
              state->mode = (c == 'c' ? NMAP : KNOCK);
              flip_images (state);
              return;
            }
        }

      screenhack_handle_event (state->dpy, &event);
    }
}


static void
matrix_usleep (m_state *state, unsigned long delay)
{
  if (!delay) return;

  if (state->cursor)
    {
      int blink_delay = 333000;
      int tot_delay = 0;
      m_cell *cursor = state->cursor;
      while (tot_delay < delay)
        {
          if (state->cursor)
            {
              usleep (blink_delay * 2);
              tot_delay += (2 * blink_delay);
              state->cursor = NULL;
            }
          else
            {
              usleep (blink_delay);
              tot_delay += blink_delay;
              state->cursor = cursor;
            }
          cursor->changed = 1;
          redraw_cells (state, False);
          XSync (state->dpy, False);
          handle_events (state);
        }
    }
  else
    {
      XSync (state->dpy, False);
      handle_events (state);
      usleep (delay);
    }
}


static void
hack_text_1 (m_state *state,
             int *xP, int *yP,
             const char *s,
             Bool typing_delay,
             Bool transmit_delay,
             Bool long_delay,
             Bool visible_cursor,
             Bool scroll_p)
{
  int x = *xP;
  int y = *yP;
  int i = state->grid_width * y + x;
  Bool glow_p = False;
  int long_delay_usecs = 1000000;

  if (long_delay == -1)
    long_delay = 0, long_delay_usecs /= 6;

  if (y >= state->grid_height-1) return;

  while (*s)
    {
      m_cell *cell;
      Bool done_p = s[1] == '\000';

      long_delay = done_p;
              
      if (*s == '\n' || x >= state->grid_width - 1)
        {
          if (*s != '\n')
            s--;
          x = 0;
          y++;
          i = state->grid_width * y + x;

          if (scroll_p)
            {
              int xx, yy;
              for (yy = 0; yy < state->grid_height-1; yy++)
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
              y--;  /* move it back */
              i = state->grid_width * y + x;
            }

          if (y >= state->grid_height) return;

          cell = &state->cells[i];
          if (visible_cursor)
            {
              cell->changed = 1;
              state->cursor = cell;
            }
        }
      else if (*s == '\010')
        ;
      else if (*s == '\002')
        glow_p = True;
      else
        {
          cell = &state->cells[i];
          if (x < state->grid_width-1)
            {
              cell->glyph = char_map[(unsigned char) *s] + 1;
              if (*s == ' ' || *s == '\t') cell->glyph = 0;
              cell->changed = 1;
              cell->glow = (glow_p ? 100 : 0);
              if (visible_cursor)
                {
                  m_cell *next = &state->cells[i + 1];
                  next->changed = 1;
                  state->cursor = next;
                }
              i++;
            }
          x++;
        }
      s++;
      if (typing_delay || transmit_delay || long_delay)
        {
          redraw_cells (state, False);
          XSync (state->dpy, False);
          handle_events (state);
          if (typing_delay)
            {
              usleep (50000);
              if (typing_delay && 0 == random() % 3)
                usleep (0xFFFFFF & ((random() % 250000) + 1));
            }
          else
            if (long_delay)
              matrix_usleep (state, long_delay_usecs);
            else
              usleep (20000);
        }
    }

  *xP = x;
  *yP = y;
}


static void
zero_cells (m_state *state)
{
  int i;
  for (i = 0; i < state->grid_height * state->grid_width; i++)
    {
      m_cell *cell = &state->cells[i];
      cell->changed = (cell->glyph != 0);
      cell->glyph   = 0;
      cell->glow    = 0;
      cell->spinner = 0;
    }
}


static void
hack_text (m_state *state)
{
  Bool typing_delay = False;
  Bool transmit_delay = False;
  Bool long_delay = False;
  Bool visible_cursor = False;

  switch (state->mode)
    {
    case KNOCK:
      {
        const char *blocks[] = {
          "Wake up, Neo...",
          "The Matrix has you...",
          "Follow the white rabbit.",
          " ",
          "Knock, knock, Neo."
        };
        int nblocks = countof(blocks);
        int j;
        typing_delay = True;
        transmit_delay = False;
        long_delay = False;
        visible_cursor = True;
        for (j = 0; j < nblocks; j++)
          {
            int x = 3;
            int y = 2;
            const char *s = blocks[j];
            if (!s[0] || !s[1]) typing_delay = False;
            zero_cells (state);
            hack_text_1 (state, &x, &y, s,
                         typing_delay, transmit_delay, -1,
                         visible_cursor, True);
            matrix_usleep (state, 2000000);
          }
      }
      break;

    case TRACEA1: case TRACEB1:
      {
        const char *blocks[10];
        int j, n = 0;

        if (state->mode == TRACEA1)
          blocks[n++] =
            (state->grid_width >= 52
             ?  "Call trans opt: received. 2-19-98 13:24:18 REC:Log>"
             : "Call trans opt: received.\n2-19-98 13:24:18 REC:Log>");
        else
          blocks[n++] =
            (state->grid_width >= 52
             ?  "Call trans opt: received. 9-18-99 14:32:21 REC:Log>"
             : "Call trans opt: received.\n9-18-99 14:32:21 REC:Log>");

        if (state->mode == TRACEB1)
          blocks[n++] = "WARNING: carrier anomaly";
        blocks[n++] = "Trace program: running";

        typing_delay = False;
        transmit_delay = True;
        long_delay = True;
        visible_cursor = True;
        for (j = 0; j < n; j++)
          {
            const char *s = blocks[j];
            int x = 0;
            int y = 0;
            zero_cells (state);
            hack_text_1 (state, &x, &y, s,
                         typing_delay, transmit_delay, long_delay,
                         visible_cursor, True);
          }
        matrix_usleep (state, 1000000);
      }
      break;

    case SYSTEMFAILURE:
      {
        const char *s = "SYSTEM FAILURE";
        int i;
        float cx = ((int)state->grid_width - (int)strlen(s)) / 2 - 0.5;
        float cy = (state->grid_height / 2) - 1.3;
        int x, y;

        if (cy < 0) cy = 0;
        if (cx < 0) cx = 0;

        XFillRectangle (state->dpy, state->window, state->erase_gc,
                        cx * state->char_width,
                        cy * state->char_height,
                        (strlen(s) + 1) * state->char_width,
                        state->char_height * 1.6);

        for (i = -2; i < 3; i++)
          {
            XGCValues gcv;
            gcv.foreground = state->colors[i + 2];
            XChangeGC (state->dpy, state->scratch_gc, GCForeground, &gcv);
            XDrawRectangle (state->dpy, state->window, state->scratch_gc,
                            cx * state->char_width - i,
                            cy * state->char_height - i,
                            (strlen(s) + 1) * state->char_width + (2 * i),
                            (state->char_height * 1.6) + (2 * i));
          }

        /* If we don't clear these out, part of the box may get overwritten */
        for (i = 0; i < state->grid_height * state->grid_width; i++)
          {
            m_cell *cell = &state->cells[i];
            cell->changed = 0;
          }

        x = ((int)state->grid_width - (int)strlen(s)) / 2;
        y = (state->grid_height / 2) - 1;
        if (y < 0) y = 0;
        if (x < 0) x = 0;
        hack_text_1 (state, &x, &y, s,
                     typing_delay, transmit_delay, long_delay,
                     visible_cursor, False);
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
        const char *blocks[] = {
          "# ",

          "\001nmap 10.2.2.2\n",
          "Starting nmap V. 2.54BETA25\n"

          "\010", "\010", "\010",

          "Insufficient responses for TCP sequencing (3), OS detection "
          "may be less accurate\n"
          "Interesting ports on 10.2.2.2:\n"
          "(The 1538 ports scanned but not shown below are in state: "
          "filtered)\n"
          "Port       state       service\n"
          "22/tcp     open        ssh\n"
          "\n"
          "No exact OS matches for host\n"
          "\n"
          "Nmap run completed -- 1 IP address (1 host up) scanned\n"
          "# ",

          "\001sshnuke 10.2.2.2 -rootpw=\"Z1ON0101\"\n",

          "Connecting to 10.2.2.2:ssh ... ",

          "successful.\n"
          "Attempting to exploit SSHv1 CRC32 ... ",

          "successful.\n"
          "Resetting root password to \"Z1ON0101\".\n",

          "System open: Access Level <9>\n"
          "# ",

          "\001ssh 10.2.2.2 -l root\n",

          "root@10.2.2.2's password: ",

          "\001\010\010\010\010\010\010\010\010\n",

          "\n"
          "RRF-CONTROL> ",

          "\001disable grid nodes 21 - 40\n",

          "\002Warning: Disabling nodes 21-40 will disconnect sector 11 "
          "(27 nodes)\n"
          "\n"
          "\002         ARE YOU SURE? (y/n) ",

          "\001\010\010y\n",
          "\n"
        };

        int nblocks = countof(blocks);
        int y = state->grid_height - 2;
        int x, j;

        visible_cursor = True;
        x = 0;
        zero_cells (state);
        for (j = 0; j < nblocks; j++)
          {
            const char *s = blocks[j];
            typing_delay = (*s == '\001');
            if (typing_delay) s++;

            long_delay = False;
            hack_text_1 (state, &x, &y, s,
                         typing_delay, transmit_delay, long_delay,
                         visible_cursor, True);
          }

        typing_delay = False;
        long_delay = False;
        for (j = 21; j <= 40; j++)
          {
            char buf[100];
            sprintf (buf, "\002Grid Node %d offline...\n", j);
            hack_text_1 (state, &x, &y, buf,
                         typing_delay, transmit_delay, -1,
                         visible_cursor, True);

          }
        long_delay = True;
        hack_text_1 (state, &x, &y, "\nRRF-CONTROL> ",
                     typing_delay, transmit_delay, long_delay,
                     visible_cursor, True);

        /* De-glow all cells before draining them... */
        for (j = 0; j < state->grid_height * state->grid_width; j++)
          {
            m_cell *cell = &state->cells[j];
            cell->changed = (cell->glow != 0);
            cell->glow = 0;
          }
      }
    break;

  default:
    abort();
    break;
  }
}


static void
drain_matrix (m_state *state)
{
  int delay = get_integer_resource ("delay", "Integer");
  int i;

  /* Fill the top row with empty top-feeders, to clear the screen. */
  for (i = 0; i < state->grid_width; i++)
    {
      m_feeder *f = &state->feeders[i];
      f->y = -1;
      f->remaining = 0;
      f->throttle = 0;
    }

  /* Turn off all the spinners, else they never go away. */
  for (i = 0; i < state->grid_width * state->grid_height; i++)
    if (state->cells[i].spinner)
      {
        state->cells[i].spinner = 0;
        state->cells[i].changed = 1;
      }

  /* Run the machine until there are no live cells left. */
  while (1)
    {
      Bool any_cells_p = False;
      for (i = 0; i < state->grid_width * state->grid_height; i++)
        if (state->cells[i].glyph)
          {
            any_cells_p = True;
            goto FOUND;
          }
    FOUND:
      if (! any_cells_p)
        return;

      feed_matrix (state);
      redraw_cells (state, True);
      XSync (state->dpy, False);
      handle_events (state);
      if (delay) usleep (delay);
    }
}


static void
roll_state (m_state *state)
{
  int delay = 0;
  switch (state->mode)
    {
    case TRACEA1:
      state->mode = TRACEA2;
      break;
    case TRACEB1:
      state->mode = TRACEB2;
      break;

    case TRACEA2:
      {
        Bool any = False;
        int i;
        for (i = 0; i < strlen(state->tracing); i++)
          if (state->tracing[i] < 0) any = True;

        if (!any)
          {
            XSync (state->dpy, False);
            matrix_usleep (state, 3000000);
            state->mode = MATRIX;
            state->glyph_map = matrix_encoding;
            state->nglyphs = countof(matrix_encoding);
            flip_images (state);
            free (state->tracing);
            state->tracing = 0;
          }
        else if ((random() % 20) == 0)  /* how fast numbers are discovered */
          {
            int x = random() % strlen(state->tracing);
            if (state->tracing[x] < 0)
              state->tracing[x] = -state->tracing[x];
          }
        break;
      }

    case TRACEB2:
      {
        /* reversed logic from TRACEA2 */
        Bool any = False;
        int i;
        for (i = 0; i < strlen(state->tracing); i++)
          if (state->tracing[i] > 0) any = True;

        if ((random() % 15) == 0) {
          if (any)
            state->mode = SYSTEMFAILURE;
          else
            {
              int x = random() % strlen(state->tracing);
              if (state->tracing[x] < 0)
                state->tracing[x] = -state->tracing[x];
            }
        }
        break;
      }

    case SYSTEMFAILURE:
      XSync (state->dpy, False);
      matrix_usleep (state, 6000000);
      state->mode = MATRIX;
      state->glyph_map = matrix_encoding;
      state->nglyphs = countof(matrix_encoding);
      flip_images (state);
      drain_matrix (state);
      matrix_usleep (state, 2000000);
      if (state->tracing) {
        free (state->tracing);
        state->tracing = 0;
      }
      break;

    case KNOCK:
    case NMAP:
      state->mode = MATRIX;
      state->glyph_map = matrix_encoding;
      state->nglyphs = countof(matrix_encoding);
      flip_images (state);
      break;

    case MATRIX:
      if (state->knock_knock_p && (! (random() % 3500)))
        {
          drain_matrix (state);
          if (! (random() % 5))
            state->mode = NMAP;
          else
            state->mode = KNOCK;

          flip_images (state);
        }
      break;

    case DNA: case BINARY: case DEC: case HEX:
      break;

    default:
      abort();
      break;
    }

  matrix_usleep (state, delay * 1000000);
  state->cursor = NULL;
}


static void
hack_matrix (m_state *state)
{
  int x;

  switch (state->mode)
    {
    case TRACEA1: case TRACEB1: case SYSTEMFAILURE:
    case KNOCK: case NMAP:
      hack_text (state);
      return;
    case TRACEA2: case TRACEB2:
    case MATRIX: case DNA: case BINARY: case DEC: case HEX:
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

      if (state->mode == TRACEA2 || state->mode == TRACEB2)
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

  if (!state->mode == TRACEA2 && !state->mode == TRACEB2 &&
      ! (random() % 500))
    init_spinners (state);
}


static void
draw_matrix (m_state *state)
{
  feed_matrix (state);
  hack_matrix (state);
  redraw_cells (state, True);
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
  ".foreground:		   #00AA00",
  "*small:		   ",
  "*delay:		   10000",
  "*insert:		   both",
  "*mode:		   Matrix",
  "*tracePhone:            (312) 555-0690",
  "*spinners:		   5",
  "*density:		   75",
  "*knockKnock:		   False",
  "*geometry:		   800x600",
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
  { "-crack",		".mode",		XrmoptionNoArg, "crack"},
  { "-phone",		".tracePhone",		XrmoptionSepArg, 0 },
  { "-dna",		".mode",		XrmoptionNoArg, "DNA" },
  { "-binary",		".mode",		XrmoptionNoArg, "binary" },
  { "-hexadecimal",	".mode",		XrmoptionNoArg, "hexadecimal"},
  { "-decimal",		".mode",		XrmoptionNoArg, "decimal"},
  { "-knock-knock",	".knockKnock",		XrmoptionNoArg, "True" },
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
      handle_events (state);
      if (delay) usleep (delay);
    }
}
