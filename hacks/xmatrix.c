/* xscreensaver, Copyright (c) 1999 Jamie Zawinski <jwz@jwz.org>
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
 * One thing I would like to add (to "-trace" mode) is an intro like at the
 * beginning of the movie, where it printed
 *
 *        Call trans opt: received. 2-19-98 13:24:18 REC:Log>_
 *
 * then cleared, then
 *
 *        Trace program: running_
 *
 * then did the trace.
 *
 * I was also thinking of sometimes making the screen go blank and say
 * "Knock, knock."  
 *
 * However, the problem with both of these ideas is, I made the number images
 * by tooling around in GIMP until I got something that looked good (blurring
 * and unblurring and enlarging and shrinking and blurring some more...) and I
 * couldn't reproduce it if my life depended on it.  And to add anything other
 * than roman digits/katakana, I'd need to matrixify a whole font...
 */

#include "screenhack.h"
#include <stdio.h>
#include <X11/Xutil.h>

#ifdef HAVE_XPM
# include <X11/xpm.h>
# include "images/matrix.xpm"
# include "images/matrix2.xpm"
#endif

#include "images/matrix.xbm"
#include "images/matrix2.xbm"

#define CHAR_ROWS 27
#define CHAR_COLS 3
#define FADE_COL  0
#define PLAIN_COL 1
#define GLOW_COL  2

typedef struct {
  unsigned int glyph   : 8;
           int glow    : 8;
  unsigned int changed : 1;
  unsigned int spinner : 1;
} m_cell;

typedef struct {
  int remaining;
  int throttle;
  int y;
} m_feeder;

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
  Bool trace_p;
  signed char *tracing;
  int density;

  Pixmap images;
  int image_width, image_height;
  int nglyphs;

} m_state;


static void
load_images (m_state *state)
{
#ifdef HAVE_XPM
  if (!get_boolean_resource ("mono", "Boolean") &&
      state->xgwa.depth > 1)
    {
      XpmAttributes xpmattrs;
      int result;
      xpmattrs.valuemask = 0;

# ifdef XpmCloseness
      xpmattrs.valuemask |= XpmCloseness;
      xpmattrs.closeness = 40000;
# endif
# ifdef XpmVisual
      xpmattrs.valuemask |= XpmVisual;
      xpmattrs.visual = state->xgwa.visual;
# endif
# ifdef XpmDepth
      xpmattrs.valuemask |= XpmDepth;
      xpmattrs.depth = state->xgwa.depth;
# endif
# ifdef XpmColormap
      xpmattrs.valuemask |= XpmColormap;
      xpmattrs.colormap = state->xgwa.colormap;
# endif

      result = XpmCreatePixmapFromData (state->dpy, state->window,
                                        (state->small_p
                                         ? matrix2_xpm
                                         : matrix_xpm),
                                        &state->images, 0 /* mask */,
                                        &xpmattrs);
      if (!state->images || (result != XpmSuccess && result != XpmColorError))
        state->images = 0;

      state->image_width = xpmattrs.width;
      state->image_height = xpmattrs.height;
      state->nglyphs = CHAR_ROWS;
    }
  else
#endif /* !HAVE_XPM */
    {
      unsigned long fg, bg;
      state->image_width =  (state->small_p ? matrix2_width  : matrix_width);
      state->image_height = (state->small_p ? matrix2_height : matrix_height);
      state->nglyphs = CHAR_ROWS;

      fg = get_pixel_resource("foreground", "Foreground",
                              state->dpy, state->xgwa.colormap);
      bg = get_pixel_resource("background", "Background",
                              state->dpy, state->xgwa.colormap);
      state->images =
        XCreatePixmapFromBitmapData (state->dpy, state->window,
                                     (state->small_p
                                      ? (char *) matrix2_bits
                                      : (char *) matrix_bits),
                                     state->image_width, state->image_height,
                                     bg, fg, state->xgwa.depth);
    }
}


static void
flip_images (m_state *state)
{
  XImage *im = XGetImage (state->dpy, state->images, 0, 0,
                          state->image_width, state->image_height,
                          ~0L, (state->xgwa.depth > 1 ? ZPixmap : XYPixmap));
  int x, y, i;
  int w = state->image_width / CHAR_COLS;
  unsigned long *row = (unsigned long *) malloc (sizeof(*row) * w);

  for (y = 0; y < state->image_height; y++)
    for (i = 0; i < CHAR_COLS; i++)
      {
        for (x = 0; x < w; x++)
          row[x] = XGetPixel (im, (i * w) + x, y);
        for (x = 0; x < w; x++)
          XPutPixel (im, (i * w) + x, y, row[w - x - 1]);
      }

  XPutImage (state->dpy, state->images, state->draw_gc, im, 0, 0, 0, 0,
             state->image_width, state->image_height);
  XDestroyImage (im);
  free (row);
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
  state->nglyphs = 10;
  flip_images (state);

  return;

 FAIL:
  fprintf (stderr, "%s: bad phone number: \"%s\".\n",
           progname, s ? s : "(null)");

  if (s) free (s);
  if (state->tracing) free (state->tracing);
  state->tracing = 0;
  state->trace_p = False;
}


static m_state *
init_matrix (Display *dpy, Window window)
{
  XGCValues gcv;
  char *insert;
  m_state *state = (m_state *) calloc (sizeof(*state), 1);
  state->dpy = dpy;
  state->window = window;

  XGetWindowAttributes (dpy, window, &state->xgwa);

  state->small_p = get_boolean_resource ("small", "Boolean");
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

  state->trace_p = get_boolean_resource ("trace", "Trace");
  if (state->trace_p)
    init_trace (state);
  else
    init_spinners (state);

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
          int g = (random() % state->nglyphs) + 1;
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
hack_matrix (m_state *state)
{
  int x;

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

      if (state->trace_p)
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

  if (!state->trace_p &&
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

        if (state->trace_p)
          {
            int xx = x % strlen(state->tracing);
            Bool dead_p = state->tracing[xx] > 0;

            if (y == 0 && x == xx)
              cell->glyph = (dead_p ? (state->tracing[xx]-'0'+1) : 0);
            else if (y == 0)
              cell->glyph = 0;
            else
              cell->glyph = (dead_p ? 0 : (random() % state->nglyphs) + 1);

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
          XCopyArea (state->dpy, state->images, state->window, state->draw_gc,
                     ((cell->glow > 0 || cell->spinner)
                      ? (state->char_width * GLOW_COL)
                      : (cell->glow == 0
                         ? (state->char_width * PLAIN_COL)
                         : (state->char_width * FADE_COL))),
                     (cell->glyph - 1) * state->char_height,
                     state->char_width, state->char_height,
                     x * state->char_width,
                     y * state->char_height);

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
            cell->glyph = random() % CHAR_ROWS;
            cell->changed = 1;
          }
      }

  if (state->trace_p)
    {
      Bool any = False;
      int i;
      for (i = 0; i < strlen(state->tracing); i++)
        if (state->tracing[i] < 0) any = True;

      if (!any)
        {
          XSync (state->dpy, False);
          sleep (3);
          state->trace_p = False;
          state->nglyphs = CHAR_ROWS;
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
    }

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
  "*small:		   False",
  "*delay:		   10000",
  "*insert:		   both",
  "*trace:		   false",
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
  { "-trace",		".trace",		XrmoptionNoArg, "True" },
  { "-phone",		".tracePhone",		XrmoptionSepArg, 0 },
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
