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

char *progclass = "Critical";


typedef struct {
  int width, height;		/* in cells */
  unsigned short *cells;
} CriticalModel;

typedef struct {
  int trail;			/* length of trail */
  int cell_size;
} CriticalSettings;


CriticalModel * model_allocate (int w, int h);
void model_initialize (CriticalModel *model);

/* Options this module understands.  */
XrmOptionDescRec options[] = {
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-colorscheme",	".colorscheme",	XrmoptionSepArg, 0 },
  { "-restart",		".restart",	XrmoptionSepArg, 0 },
  { "-cellsize",	".cellsize",	XrmoptionSepArg, 0 },
  { "-batchcount",	".batchcount",  XrmoptionSepArg, 0 },
  { "-trail",		".trail",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }		/* end */
};


/* Default xrm resources. */
char *defaults[] = {
  ".background:			black",
  "*colorscheme:		smooth",
  "*delay:			10000", 
  "*ncolors:			64",
  "*restart:			8",
  "*batchcount:			1500",
  "*trail:			50",
  0				/* end */
};


int
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

CriticalModel *
model_allocate (int model_w, int model_h)
{
  CriticalModel		*model;

  model = malloc (sizeof (CriticalModel));
  if (!model)
    return 0;

  model->width = model_w;
  model->height = model_h;

  model->cells = malloc (sizeof (unsigned short) * model_w * model_h);
  if (!model->cells)
    return 0;

  return model;
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


void
model_initialize (CriticalModel *model)
{
  int i;
  
  for (i = model->width * model->height - 1; i >= 0; i--)
    {
      model->cells[i] = (unsigned short) random ();
    }
}


/* Move one step forward in the criticality simulation.

   This function locates and returns in (TOP_X, TOP_Y) the location of
   the highest-valued cell in the model.  It also replaces that cell
   and it's eight nearest neighbours with new random values.
   Neighbours that fall off the edge of the model are simply
   ignored. */
static void
model_step (CriticalModel *model, XPoint *ptop)
{
  int			x, y, i;
  int			dx, dy;
  unsigned short	top_value = 0;
  int			top_x = 0, top_y = 0;

  /* Find the top cell */
  top_value = 0;
  i = 0;
  for (y = 0; y < model->height; y++)
    for (x = 0; x < model->width; x++)
      {
	if (model->cells[i] >= top_value)
	  {
	    top_value = model->cells[i];
	    top_x = x;
	    top_y = y;
	  }
	i++;
      }

  /* Replace it and its neighbours with new random values */
  for (dy = -1; dy <= 1; dy++)
    {
      int y = top_y + dy;
      if (y < 0  ||  y >= model->height)
	continue;
      
      for (dx = -1; dx <= 1; dx++)
	{
	  int x = top_x + dx;
	  if (x < 0  ||  x >= model->width)
	    continue;
	  
	  model->cells[y * model->width + x] = (unsigned short) random();
	}
    }

  ptop->x = top_x;
  ptop->y = top_y;
}


/* Construct and return in COLORS and N_COLORS a new set of colors,
   depending on the resource settings.  */
void
setup_colormap (Display *dpy, XWindowAttributes *wattr,
		XColor **colors,
		int *n_colors)
{
  Bool			writable;
  char const *		color_scheme;

  /* Make a colormap */
  *n_colors = get_integer_resource ("ncolors", "Integer");
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
  color_scheme = get_string_resource ("colorscheme", "ColorScheme");
  
  if (!strcmp (color_scheme, "random"))
    {
      make_random_colormap (dpy, wattr->visual,
			    wattr->colormap,
			    *colors, n_colors,
			    True, True, &writable, True);
    }
  else if (!strcmp (color_scheme, "smooth"))
    {
      make_smooth_colormap (dpy, wattr->visual,
			    wattr->colormap,
			    *colors, n_colors,
			    True, &writable, True);
    }
  else 
    {
      make_uniform_colormap (dpy, wattr->visual,
			     wattr->colormap,
			     *colors, n_colors, True,
			     &writable, True);
    }
}


/* Free allocated colormap created by setup_colormap. */
static void
free_colormap (Display *dpy, XWindowAttributes *wattr,
               XColor **colors, int n_colors)
{
  free_colors (dpy, wattr->colormap, *colors, n_colors);
  free (*colors);
}



/* Draw one step of the hack.  Positions are cell coordinates. */
static void
draw_step (CriticalSettings *settings,
	   Display *dpy, Window window, GC gc,
	   int pos, XPoint *history)
{
  int cell_size = settings->cell_size;
  int half = cell_size/2;
  int old_pos = (pos + settings->trail - 1) % settings->trail;
  
  pos = pos % settings->trail;

  XDrawLine (dpy, window, gc, 
	     history[pos].x * cell_size + half,
	     history[pos].y * cell_size + half,
	     history[old_pos].x * cell_size + half,
	     history[old_pos].y * cell_size + half);
}



/* Display a self-organizing criticality screen hack.  The program
   runs indefinately on the root window. */
void
screenhack (Display *dpy, Window window)
{
  int			n_colors;
  XColor		*colors;
  int			model_w, model_h;
  CriticalModel		*model;
  int			lines_per_color = 10;
  int			i_color = 0;
  long			delay_usecs;
  int			batchcount;
  XPoint		*history; /* in cell coords */
  int			pos = 0;
  int			wrapped = 0;
  GC			fgc, bgc;
  XGCValues		gcv;
  XWindowAttributes	wattr;
  CriticalSettings	settings;

  /* Number of screens that should be drawn before reinitializing the
     model, and count of the number of screens done so far. */
  int			n_restart, i_restart;

  /* Find window attributes */
  XGetWindowAttributes (dpy, window, &wattr);

  batchcount = get_integer_resource ("batchcount", "Integer");
  if (batchcount < 5)
    batchcount = 5;

  /* For the moment the model size is just fixed -- making it vary
     with the screen size just makes the hack boring on large
     screens. */
  model_w = 80;
  settings.cell_size = wattr.width / model_w;
  model_h = wattr.height / settings.cell_size;

  /* Construct the initial model state. */

  settings.trail = clip(2, get_integer_resource ("trail", "Integer"), 1000);
  
  history = malloc (sizeof history[0] * settings.trail);
  if (!history)
    {
      fprintf (stderr, "critical: "
	       "couldn't allocate trail history of %d cells\n",
	       settings.trail);
      return;
    }

  model = model_allocate (model_w, model_h);
  if (!model)
    {
      fprintf (stderr, "critical: error preparing the model\n");
      return;
    }
  
  /* make a black gc for the background */
  gcv.foreground = get_pixel_resource ("background", "Background",
				       dpy, wattr.colormap);
  bgc = XCreateGC (dpy, window, GCForeground, &gcv);

  fgc = XCreateGC (dpy, window, 0, &gcv);

  delay_usecs = get_integer_resource ("delay", "Integer");
  n_restart = get_integer_resource ("restart", "Integer");
    
  /* xscreensaver will kill or stop us when the user does something
   * that deserves attention. */
  i_restart = 0;
  
  while (1) {
    int i_batch;

    if (i_restart == 0)
      {
	/* Time to start a new simulation, this one has probably got
	   to be a bit boring. */
	setup_colormap (dpy, &wattr, &colors, &n_colors);
	erase_full_window (dpy, window);
	model_initialize (model);
	model_step (model, &history[0]);
	pos = 1;
	wrapped = 0;
      }
    
    for (i_batch = batchcount; i_batch; i_batch--)
      {
	/* Set color */
	if ((i_batch % lines_per_color) == 0)
	  {
	    i_color = (i_color + 1) % n_colors;
	    gcv.foreground = colors[i_color].pixel;
	    XChangeGC (dpy, fgc, GCForeground, &gcv);
	  }
	
	assert(pos >= 0 && pos < settings.trail);
	model_step (model, &history[pos]);

	draw_step (&settings, dpy, window, fgc, pos, history);

	/* we use the history as a ring buffer, but don't start erasing until
	   we've wrapped around once. */
	if (++pos >= settings.trail)
	  {
	    pos -= settings.trail;
	    wrapped = 1;
	  }

	if (wrapped)
	  {
	    draw_step (&settings, dpy, window, bgc, pos+1, history);
	  }

	XSync (dpy, False); 
	screenhack_handle_events (dpy);
	
	if (delay_usecs)
	  usleep (delay_usecs);

      }
    
    i_restart = (i_restart + 1) % n_restart;

    if (i_restart == 0)
      {
	/* Clean up after completing a simulation. */
	free_colormap (dpy, &wattr, &colors, n_colors);
      }
  }
}
