/* critical -- Self-organizing-criticality display hack for XScreenSaver
 * Copyright (C) 1998, 1999, 2000 Martin Pool <mbp@humbug.org.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.  No representations are made
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * See `critical.man' for more information.
 *
 * Revision history:
 * 13 Nov 1998: Initial version, Martin Pool <mbp@humbug.org.au>
 * 08 Feb 2000: Change to keeping and erasing a trail, <mbp>
 *
 * It would be nice to draw curvy shapes rather than just straight
 * lines, but X11 doesn't have spline primitives (?) so we'd have to
 * do all the work ourselves  */

#include "screenhack.h"
#include "erase.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


typedef struct {
  int width, height;		/* in cells */
  unsigned short *cells;
} CriticalModel;

typedef struct {
  int trail;			/* length of trail */
  int cell_size;
} CriticalSettings;


/* Number of screens that should be drawn before reinitializing the
   model, and count of the number of screens done so far. */

struct state {
  Display *dpy;
  Window window;

  int n_restart, i_restart;
  XWindowAttributes	wattr;
  CriticalModel		*model;
  int			batchcount;
  XPoint		*history; /* in cell coords */
  long			delay_usecs;
  GC			fgc, bgc;
  XGCValues		gcv;
  CriticalSettings	settings;

  int			d_n_colors;
  XColor			*d_colors;
  int			lines_per_color;
  int			d_i_color;
  int			d_pos;
  int			d_wrapped;

  int d_i_batch;
  eraser_state *eraser;

};


static CriticalModel * model_allocate (int w, int h);
static void model_initialize (CriticalModel *);


static int
clip (int low, int val, int high)
{
  if (val < low)
    return low;
  else if (val > high)
    return high;
  else
    return val;
}


/* Allocate an return a new simulation model datastructure.
 */

static CriticalModel *
model_allocate (int model_w, int model_h)
{
  CriticalModel		*mm;

  mm = malloc (sizeof (CriticalModel));
  if (!mm)
    return 0;

  mm->width = model_w;
  mm->height = model_h;

  mm->cells = malloc (sizeof (unsigned short) * model_w * model_h);
  if (!mm->cells)
    return 0;

  return mm;
}



/* Initialize the data model underlying the hack.

   For the self-organizing criticality hack, this consists of a 2d
   array full of random integers.

   I've considered storing the data as (say) a binary tree within a 2d
   array, to make finding the highest value faster at the expense of
   storage space: searching the whole array on each iteration seems a
   little inefficient.  However, the screensaver doesn't seem to take
   up many cycles as it is: presumably the search is pretty quick
   compared to the sleeps.  The current version uses less than 1% of
   the CPU time of an AMD K6-233.  Many machines running X11 at this
   point in time seem to be memory-limited, not CPU-limited.

   The root of all evil, and all that.
*/


static void
model_initialize (CriticalModel *mm)
{
  int i;
  
  for (i = mm->width * mm->height - 1; i >= 0; i--)
    {
      mm->cells[i] = (unsigned short) random ();
    }
}


/* Move one step forward in the criticality simulation.

   This function locates and returns in (TOP_X, TOP_Y) the location of
   the highest-valued cell in the model.  It also replaces that cell
   and it's eight nearest neighbours with new random values.
   Neighbours that fall off the edge of the model are simply
   ignored. */
static void
model_step (CriticalModel *mm, XPoint *ptop)
{
  int			x, y, i;
  int			dx, dy;
  unsigned short	top_value = 0;
  int			top_x = 0, top_y = 0;

  /* Find the top cell */
  top_value = 0;
  i = 0;
  for (y = 0; y < mm->height; y++)
    for (x = 0; x < mm->width; x++)
      {
	if (mm->cells[i] >= top_value)
	  {
	    top_value = mm->cells[i];
	    top_x = x;
	    top_y = y;
	  }
	i++;
      }

  /* Replace it and its neighbours with new random values */
  for (dy = -1; dy <= 1; dy++)
    {
      int yy = top_y + dy;
      if (yy < 0  ||  yy >= mm->height)
	continue;
      
      for (dx = -1; dx <= 1; dx++)
	{
	  int xx = top_x + dx;
	  if (xx < 0  ||  xx >= mm->width)
	    continue;
	  
	  mm->cells[yy * mm->width + xx] = (unsigned short) random();
	}
    }

  ptop->x = top_x;
  ptop->y = top_y;
}


/* Construct and return in COLORS and N_COLORS a new set of colors,
   depending on the resource settings.  */
static void
setup_colormap (struct state *st, XColor **colors, int *n_colors)
{
  Bool			writable;
  char const *		color_scheme;

  /* Make a colormap */
  *n_colors = get_integer_resource (st->dpy, "ncolors", "Integer");
  if (*n_colors < 3)
    *n_colors = 3;
  
  *colors = (XColor *) calloc (sizeof(XColor), *n_colors);
  if (!*colors)
    {
      fprintf (stderr, "%s:%d: can't allocate memory for colors\n",
	       __FILE__, __LINE__);
      return;
    }

  writable = False;
  color_scheme = get_string_resource (st->dpy, "colorscheme", "ColorScheme");
  
  if (!strcmp (color_scheme, "random"))
    {
      make_random_colormap (st->dpy, st->wattr.visual,
			    st->wattr.colormap,
			    *colors, n_colors,
			    True, True, &writable, True);
    }
  else if (!strcmp (color_scheme, "smooth"))
    {
      make_smooth_colormap (st->dpy, st->wattr.visual,
			    st->wattr.colormap,
			    *colors, n_colors,
			    True, &writable, True);
    }
  else 
    {
      make_uniform_colormap (st->dpy, st->wattr.visual,
			     st->wattr.colormap,
			     *colors, n_colors, True,
			     &writable, True);
    }
}


/* Free allocated colormap created by setup_colormap. */
static void
free_colormap (struct state *st, XColor **colors, int n_colors)
{
  free_colors (st->dpy, st->wattr.colormap, *colors, n_colors);
  free (*colors);
}



/* Draw one step of the hack.  Positions are cell coordinates. */
static void
draw_step (struct state *st, GC gc, int pos)
{
  int cell_size = st->settings.cell_size;
  int half = cell_size/2;
  int old_pos = (pos + st->settings.trail - 1) % st->settings.trail;
  
  pos = pos % st->settings.trail;

  XDrawLine (st->dpy, st->window, gc, 
	     st->history[pos].x * cell_size + half,
	     st->history[pos].y * cell_size + half,
	     st->history[old_pos].x * cell_size + half,
	     st->history[old_pos].y * cell_size + half);
}



static void *
critical_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  int			model_w, model_h;
  st->dpy = dpy;
  st->window = window;

  /* Find window attributes */
  XGetWindowAttributes (st->dpy, st->window, &st->wattr);

  st->batchcount = get_integer_resource (st->dpy, "batchcount", "Integer");
  if (st->batchcount < 5)
    st->batchcount = 5;

  st->lines_per_color = 10;

  /* For the moment the model size is just fixed -- making it vary
     with the screen size just makes the hack boring on large
     screens. */
  model_w = 80;
  st->settings.cell_size = st->wattr.width / model_w;
  model_h = st->settings.cell_size ?
    st->wattr.height / st->settings.cell_size : 0;

  /* Construct the initial model state. */

  st->settings.trail = clip(2, get_integer_resource (st->dpy, "trail", "Integer"), 1000);
  
  st->history = calloc (st->settings.trail, sizeof (st->history[0]));
  if (!st->history)
    {
      fprintf (stderr, "critical: "
	       "couldn't allocate trail history of %d cells\n",
	       st->settings.trail);
      abort();
    }

  st->model = model_allocate (model_w, model_h);
  if (!st->model)
    {
      fprintf (stderr, "critical: error preparing the model\n");
      abort();
    }
  
  /* make a black gc for the background */
  st->gcv.foreground = get_pixel_resource (st->dpy, st->wattr.colormap,
                                       "background", "Background");
  st->bgc = XCreateGC (st->dpy, st->window, GCForeground, &st->gcv);

  st->fgc = XCreateGC (st->dpy, st->window, 0, &st->gcv);

#ifdef HAVE_COCOA
  jwxyz_XSetAntiAliasing (dpy, st->fgc, False);
  jwxyz_XSetAntiAliasing (dpy, st->bgc, False);
#endif

  st->delay_usecs = get_integer_resource (st->dpy, "delay", "Integer");
  st->n_restart = get_integer_resource (st->dpy, "restart", "Integer");
    
  setup_colormap (st, &st->d_colors, &st->d_n_colors);
  model_initialize (st->model);
  model_step (st->model, &st->history[0]);
  st->d_pos = 1;
  st->d_wrapped = 0;
  st->i_restart = 0;
  st->d_i_batch = st->batchcount;

  return st;
}

static unsigned long
critical_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  if (st->eraser) {
    st->eraser = erase_window (st->dpy, st->window, st->eraser);
    return st->delay_usecs;
  }

  /* for (d_i_batch = batchcount; d_i_batch; d_i_batch--) */
    {
      /* Set color */
      if ((st->d_i_batch % st->lines_per_color) == 0)
        {
          st->d_i_color = (st->d_i_color + 1) % st->d_n_colors;
          st->gcv.foreground = st->d_colors[st->d_i_color].pixel;
          XChangeGC (st->dpy, st->fgc, GCForeground, &st->gcv);
        }
	
      assert(st->d_pos >= 0 && st->d_pos < st->settings.trail);
      model_step (st->model, &st->history[st->d_pos]);

      draw_step (st, st->fgc, st->d_pos);

      /* we use the history as a ring buffer, but don't start erasing until
         we've d_wrapped around once. */
      if (++st->d_pos >= st->settings.trail)
        {
          st->d_pos -= st->settings.trail;
          st->d_wrapped = 1;
        }

      if (st->d_wrapped)
        {
          draw_step (st, st->bgc, st->d_pos+1);
        }

    }
    
  st->d_i_batch--;
  if (st->d_i_batch < 0)
    st->d_i_batch = st->batchcount;
  else
    return st->delay_usecs;

  st->i_restart = (st->i_restart + 1) % st->n_restart;

  if (st->i_restart == 0)
    {
      /* Time to start a new simulation, this one has probably got
         to be a bit boring. */
      free_colormap (st, &st->d_colors, st->d_n_colors);
      setup_colormap (st, &st->d_colors, &st->d_n_colors);
      st->eraser = erase_window (st->dpy, st->window, st->eraser);
      model_initialize (st->model);
      model_step (st->model, &st->history[0]);
      st->d_pos = 1;
      st->d_wrapped = 0;
      st->d_i_batch = st->batchcount;
    }

  return st->delay_usecs;
}

static void
critical_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
critical_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
critical_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


/* Options this module understands.  */
static XrmOptionDescRec critical_options[] = {
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-colorscheme",	".colorscheme",	XrmoptionSepArg, 0 },
  { "-restart",		".restart",	XrmoptionSepArg, 0 },
  { "-batchcount",	".batchcount",  XrmoptionSepArg, 0 },
  { "-trail",		".trail",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }		/* end */
};


/* Default xrm resources. */
static const char *critical_defaults[] = {
  ".background:			black",
  "*fpsSolid:			true",
  "*colorscheme:		smooth",
  "*delay:			10000", 
  "*ncolors:			64",
  "*restart:			8",
  "*batchcount:			1500",
  "*trail:			50",
  0				/* end */
};


XSCREENSAVER_MODULE ("Critical", critical)
