/* critical -- Self-organizing-criticality display hack for XScreenSaver
 * Copyright (C) 1998, 1999 Martin Pool <mbp@humbug.org.au>
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
 */

#include "screenhack.h"
#include "erase.h"

#include <stdio.h>
#include <stdlib.h>

char *progclass = "Critical";


typedef struct {
  int width, height;		/* in cells */
  unsigned short *cells;
} CriticalModel;


CriticalModel * model_allocate (int w, int h);
void model_initialize (CriticalModel *model);
static void model_step (CriticalModel *model, int *top_x, int *top_y);


/* Options this module understands.  */
XrmOptionDescRec options[] = {
  { "-ncolors",		".ncolors",	XrmoptionSepArg, 0 },
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-colorscheme",	".colorscheme",	XrmoptionSepArg, 0 },
  { "-restart",		".restart",	XrmoptionSepArg, 0 },
  { "-cellsize",	".cellsize",	XrmoptionSepArg, 0 },
  { "-batchcount",	".batchcount",  XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }		/* end */
};


/* Default xrm resources. */
char *defaults[] = {
  ".background:			black",
  "*colorscheme:		smooth",
  "*delay:			10000", 
  "*ncolors:			64",
  "*restart:			8",
  "*cellsize:			9",
  "*batchcount:			1500",
  0				/* end */
};


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

  model->cells = malloc (sizeof (int) * model_w * model_h);
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
  
  for (i = model->width * model->height; i >= 0; i--)
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
model_step (CriticalModel *model, int *top_x, int *top_y)
{
  int			x, y, i;
  int			dx, dy;
  unsigned short	top_value;

  /* Find the top cell */
  top_value = 0;
  i = 0;
  for (y = 0; y < model->height; y++)
    for (x = 0; x < model->width; x++)
      {
	if (model->cells[i] >= top_value)
	  {
	    top_value = model->cells[i];
	    *top_x = x;
	    *top_y = y;
	  }
	i++;
      }

  /* Replace it and its neighbours with new random values */
  for (dy = -1; dy <= 1; dy++)
    {
      int y = *top_y + dy;
      if (y < 0  ||  y >= model->height)
	continue;
      
      for (dx = -1; dx <= 1; dx++)
	{
	  int x = *top_x + dx;
	  if (x < 0  ||  x >= model->width)
	    continue;
	  
	  model->cells[y * model->width + x] = (unsigned short) random();
	}
    }
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
  if (*n_colors < 2)
    *n_colors = 2;
  
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



/* Display a self-organizing criticality screen hack.  The program
   runs indefinately on the root window. */
void
screenhack (Display *dpy, Window window)
{
  GC			fgc, bgc;
  XGCValues		gcv;
  XWindowAttributes	wattr;
  int			n_colors;
  XColor		*colors;
  int			model_w, model_h;
  CriticalModel		*model;
  int			lines_per_color = 10;
  int			i_color = 0;
  int			x1, y1, x2, y2;
  long			delay_usecs;
  int			cell_size;
  int			batchcount;

  /* Number of screens that should be drawn before reinitializing the
     model, and count of the number of screens done so far. */
  int			n_restart, i_restart;

  /* Find window attributes */
  XGetWindowAttributes (dpy, window, &wattr);

  /* Construct the initial model state. */
  cell_size = get_integer_resource ("cellsize", "Integer");
  if (cell_size < 1)
    cell_size = 1;
  if (cell_size >= 100)
    cell_size = 99;

  batchcount = get_integer_resource ("batchcount", "Integer");
  if (batchcount < 5)
    batchcount = 5;
  
  model_w = wattr.width / cell_size;
  model_h = wattr.height  / cell_size;
  
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

  x2 = rand() % model_w;
  y2 = rand() % model_h;

  delay_usecs = get_integer_resource ("delay", "Integer");
  n_restart = get_integer_resource ("restart", "Integer");
    
  /* xscreensaver will kill or stop us when the user does something
   * that deserves attention. */
  i_restart = 0;
  
  while (1) {
    int i_batch;

    if (!i_restart)
      {
	setup_colormap (dpy, &wattr, &colors, &n_colors);
	model_initialize (model);
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
	
      /* draw a line */
      x1 = x2;
      y1 = y2;

      model_step (model, &x2, &y2);

      XDrawLine (dpy, window, fgc,
		 x1 * cell_size + cell_size/2,
		 y1 * cell_size + cell_size/2,
		 x2 * cell_size + cell_size/2,
		 y2 * cell_size + cell_size/2);

      XSync (dpy, False); 
      screenhack_handle_events (dpy);

      if (delay_usecs)
        usleep (delay_usecs);
    }

    i_restart = (i_restart + 1) % n_restart;
    erase_full_window (dpy, window);
  }
}


/*
 * Local variables:
 * c-indent-mode: gnu
 * compile-command "make critical && ./critical"
 * End:
 */
