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
 */

#include "screenhack.h"
#include <stdio.h>
#include <X11/Xutil.h>

#ifdef HAVE_XPM
# include <X11/xpm.h>
# include "images/matrix.xpm"
#endif

#include "images/matrix.xbm"

#define CHAR_HEIGHT 31

typedef struct {
  int glyph;
  Bool changed;
  int glow;
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
  Bool insert_top_p, insert_bottom_p;
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

      result = XpmCreatePixmapFromData (state->dpy, state->window, matrix,
                                        &state->images, 0 /* mask */,
                                        &xpmattrs);
      if (!state->images || (result != XpmSuccess && result != XpmColorError))
        state->images = 0;

      state->image_width = xpmattrs.width;
      state->image_height = xpmattrs.height;
      state->nglyphs = state->image_height / CHAR_HEIGHT;
    }
  else
#endif /* !HAVE_XPM */
    {
      unsigned long fg, bg;
      state->image_width = matrix_width;
      state->image_height = matrix_height;
      state->nglyphs = state->image_height / CHAR_HEIGHT;

      fg = get_pixel_resource("foreground", "Foreground",
                              state->dpy, state->xgwa.colormap);
      bg = get_pixel_resource("background", "Background",
                              state->dpy, state->xgwa.colormap);
      state->images =
        XCreatePixmapFromBitmapData (state->dpy, state->window,
                                     (char *) matrix_bits,
                                     state->image_width, state->image_height,
                                     bg, fg, state->xgwa.depth);
    }
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

  state->char_width = state->image_width / 2;
  state->char_height = CHAR_HEIGHT;

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
                 "%s: `insert' must be `top', `bottom', or `both', not \%s'\n",
                 progname, insert);
      state->insert_top_p = False;
      state->insert_bottom_p = True;
    }

  if (insert)
    free (insert);

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
          *to = *from;
          to->changed = True;
        }
      to = &state->cells[x];
    }

  to->glyph = glyph;
  to->changed = True;

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
              cell->changed = True;
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

      if (state->insert_top_p && state->insert_bottom_p)
        bottom_feeder_p = (random() & 1);
      else
        bottom_feeder_p = state->insert_bottom_p;

      if (bottom_feeder_p)
        f->y = random() % (state->grid_height / 2);
      else
        f->y = -1;
    }
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
                     (cell->glow ? state->char_width : 0),
                     (cell->glyph - 1) * state->char_height,
                     state->char_width, state->char_height,
                     x * state->char_width,
                     y * state->char_height);

        cell->changed = False;

        if (cell->glow > 0)
          {
            cell->glow--;
            cell->changed = True;
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
  "*delay:		   10000",
  "*insert:		   both",
  "*density:		   85",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-top",		".insert",		XrmoptionNoArg, "top" },
  { "-bottom",		".insert",		XrmoptionNoArg, "bottom" },
  { "-both",		".insert",		XrmoptionNoArg, "both" },
  { "-density",		".density",		XrmoptionSepArg, 0 },
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
