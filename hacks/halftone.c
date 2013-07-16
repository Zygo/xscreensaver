/* halftone, Copyright (c) 2002 by Peter Jaric <peter@jaric.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Description:
 * Draws the gravitational force in each point on the screen seen
 * through a halftone dot pattern. The force is calculated from a set
 * of moving mass points. View it from a distance for best effect.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "screenhack.h"

#define DEFAULT_DELAY          10000
#define DEFAULT_SPACING        14
#define DEFAULT_SIZE_FACTOR    1.5
#define DEFAULT_COUNT          10
#define DEFAULT_MIN_MASS       0.001
#define DEFAULT_MAX_MASS       0.02
#define DEFAULT_MIN_SPEED      0.001
#define DEFAULT_MAX_SPEED      0.02


typedef struct
{
  /* halftone dots */
  double * dots;
  int dots_width;
  int dots_height;
  int spacing; 
  int max_dot_size;

  /* Moving gravity points */
  int gravity_point_count;

  double* gravity_point_x;
  double* gravity_point_y;
  double* gravity_point_mass;
  double* gravity_point_x_inc;
  double* gravity_point_y_inc;

  /* X stuff */
  Display *dpy;
  Window window;
  GC gc;

  int ncolors;
  XColor *colors;
  int color0, color1;
  int color_tick, cycle_speed;

  /* Off screen buffer */
  Pixmap buffer;
  GC buffer_gc;
  int buffer_width;
  int buffer_height;

  int delay;

} halftone_screen;


static void update_buffer(halftone_screen *halftone, XWindowAttributes * attrs)
{
  if (halftone->buffer_width != attrs->width ||
      halftone->buffer_height != attrs->height)
  {
    XGCValues gc_values;

    if (halftone->buffer_width != -1 &&
	halftone->buffer_height != -1)
    {
      if (halftone->buffer != halftone->window)
        XFreePixmap(halftone->dpy, halftone->buffer);
      XFreeGC(halftone->dpy, halftone->buffer_gc);
    }

    halftone->buffer_width = attrs->width;
    halftone->buffer_height = attrs->height;
#ifdef HAVE_COCOA	/* Don't second-guess Quartz's double-buffering */
    halftone->buffer = halftone->window;
#else
    halftone->buffer = XCreatePixmap(halftone->dpy, halftone->window, halftone->buffer_width, halftone->buffer_height, attrs->depth);
#endif

    halftone->buffer_gc = XCreateGC(halftone->dpy, halftone->buffer, 0, &gc_values);
  }
}

static void update_dot_attributes(halftone_screen *halftone, XWindowAttributes * attrs)
{  
  double dots_width = attrs->width / halftone->spacing + 1;
  double dots_height = attrs->height / halftone->spacing + 1;

  if (halftone->dots == NULL ||
      (dots_width != halftone->dots_width ||
       dots_height != halftone->dots_height))
  {
    if (halftone->dots != NULL)
      free(halftone->dots);

    halftone->dots_width = dots_width;
    halftone->dots_height = dots_height;
    halftone->dots = (double *) malloc(halftone->dots_width * halftone->dots_height * sizeof(double));
  }
}

static void *
halftone_init (Display *dpy, Window window)
{
  int x, y, i;
  int count;
  int spacing;
  double factor;
  double min_mass;
  double max_mass;
  double min_speed;
  double max_speed;
  XGCValues gc_values;
  XWindowAttributes attrs;
  halftone_screen *halftone;

  halftone = (halftone_screen *) calloc (1, sizeof(halftone_screen));

  halftone->dpy = dpy;
  halftone->window = window;

  halftone->delay = get_integer_resource (dpy, "delay", "Integer");
  halftone->delay = (halftone->delay < 0 ? DEFAULT_DELAY : halftone->delay);

  halftone->gc = XCreateGC (halftone->dpy, halftone->window, 0, &gc_values);

  halftone->buffer_width = -1;
  halftone->buffer_height = -1;
  halftone->dots = NULL;

  /* Read command line arguments and set all settings. */ 
  count = get_integer_resource (dpy, "count", "Count");
  halftone->gravity_point_count = count < 1 ? DEFAULT_COUNT : count; 

  spacing = get_integer_resource (dpy, "spacing", "Integer");
  halftone->spacing = spacing < 1 ? DEFAULT_SPACING : spacing; 

  factor = get_float_resource (dpy, "sizeFactor", "Double");
  halftone->max_dot_size = 
    (factor < 0 ? DEFAULT_SIZE_FACTOR : factor) * halftone->spacing; 

  min_mass = get_float_resource (dpy, "minMass", "Double");
  min_mass = min_mass < 0 ? DEFAULT_MIN_MASS : min_mass;

  max_mass = get_float_resource (dpy, "maxMass", "Double");
  max_mass = max_mass < 0 ? DEFAULT_MAX_MASS : max_mass;
  max_mass = max_mass < min_mass ? min_mass : max_mass;

  min_speed = get_float_resource (dpy, "minSpeed", "Double");
  min_speed = min_speed < 0 ? DEFAULT_MIN_SPEED : min_speed;

  max_speed = get_float_resource (dpy, "maxSpeed", "Double");
  max_speed = max_speed < 0 ? DEFAULT_MAX_SPEED : max_speed;
  max_speed = max_speed < min_speed ? min_speed : max_speed;


  /* Set up the moving gravity points. */
  halftone->gravity_point_x = (double *) malloc(halftone->gravity_point_count * sizeof(double));
  halftone->gravity_point_y = (double *) malloc(halftone->gravity_point_count * sizeof(double));
  halftone->gravity_point_mass = (double *) malloc(halftone->gravity_point_count * sizeof(double));
  halftone->gravity_point_x_inc = (double *) malloc(halftone->gravity_point_count * sizeof(double));
  halftone->gravity_point_y_inc = (double *) malloc(halftone->gravity_point_count * sizeof(double));

  for (i = 0; i < halftone->gravity_point_count; i++)
  {
    halftone->gravity_point_x[i] = frand(1);
    halftone->gravity_point_y[i] = frand(1);
    halftone->gravity_point_mass[i] = min_mass + (max_mass - min_mass) * frand(1);
    halftone->gravity_point_x_inc[i] = min_speed + (max_speed - min_speed) * frand(1);
    halftone->gravity_point_y_inc[i] = min_speed + (max_speed - min_speed) * frand(1);
  }


  /* Set up the dots. */
  XGetWindowAttributes(halftone->dpy, halftone->window, &attrs);  

  halftone->ncolors = get_integer_resource (dpy, "colors", "Colors");
  if (halftone->ncolors < 4) halftone->ncolors = 4;
  halftone->colors = (XColor *) calloc(halftone->ncolors, sizeof(XColor));
  make_smooth_colormap (attrs.screen, attrs.visual, attrs.colormap,
                        halftone->colors, &halftone->ncolors,
                        True, 0, False);
  halftone->color0 = 0;
  halftone->color1 = halftone->ncolors / 2;
  halftone->cycle_speed = get_integer_resource (dpy, "cycleSpeed", "CycleSpeed");
  halftone->color_tick = 0;

  update_buffer(halftone, &attrs);
  update_dot_attributes(halftone, &attrs);

  for (x = 0; x < halftone->dots_width; x++)
    for (y = 0; y < halftone->dots_height; y++)
    {
	halftone->dots[x + y * halftone->dots_width] = 0;
    }

  return halftone;
}



static void fill_circle(Display *dpy, Window window, GC gc, int x, int y, int size)
{
  int start_x = x - (size / 2);
  int start_y = y - (size / 2);
  unsigned int width = size;
  unsigned int height = size;
  int angle1 = 0;
  int angle2 = 360 * 64; /* A full circle */

  XFillArc (dpy, window, gc,
	    start_x, start_y, width, height,
	    angle1, angle2);
}

static void repaint_halftone(halftone_screen *halftone)
{
  int x, y;
  /*
  int x_offset = halftone->spacing / 2;
  int y_offset = halftone->spacing / 2;
  */
  int x_offset = 0;
  int y_offset = 0;

  
  /* Fill buffer with background color */
  XSetForeground (halftone->dpy, halftone->buffer_gc,
                  halftone->colors[halftone->color0].pixel);
  XFillRectangle(halftone->dpy, halftone->buffer, halftone->buffer_gc, 0, 0, halftone->buffer_width, halftone->buffer_height);

  /* Draw dots on buffer */
  XSetForeground (halftone->dpy, halftone->buffer_gc,
                  halftone->colors[halftone->color1].pixel);

  if (halftone->color_tick++ >= halftone->cycle_speed)
    {
      halftone->color_tick = 0;
      halftone->color0 = (halftone->color0 + 1) % halftone->ncolors;
      halftone->color1 = (halftone->color1 + 1) % halftone->ncolors;
    }

  for (x = 0; x < halftone->dots_width; x++)
    for (y = 0; y < halftone->dots_height; y++)
      fill_circle(halftone->dpy, halftone->buffer, halftone->buffer_gc,
		  x_offset + x * halftone->spacing, y_offset + y * halftone->spacing, 
		  halftone->max_dot_size * halftone->dots[x + y * halftone->dots_width]);

  /* Copy buffer to window */
  if (halftone->buffer != halftone->window)
    XCopyArea(halftone->dpy, halftone->buffer, halftone->window, halftone->gc, 0, 0, halftone->buffer_width, halftone->buffer_height, 0, 0);
}

static double calculate_gravity(halftone_screen *halftone, int x, int y)
{
  int i;
  double gx = 0;
  double gy = 0;

  for (i = 0; i < halftone->gravity_point_count; i++)
  {
    double dx = ((double) x) - halftone->gravity_point_x[i] * halftone->dots_width; 
    double dy = ((double) y) - halftone->gravity_point_y[i] * halftone->dots_height; 
    double distance = sqrt(dx * dx + dy * dy);
    
    if (distance != 0)
    {
      double gravity = halftone->gravity_point_mass[i] / (distance * distance  / (halftone->dots_width * halftone->dots_height));
      
      gx += (dx / distance) * gravity;
      gy += (dy / distance) * gravity;
    }
  }  
  
  return sqrt(gx * gx + gy * gy);
}

static void update_halftone(halftone_screen *halftone)
{
  int x, y, i;
  XWindowAttributes attrs;

  XGetWindowAttributes(halftone->dpy, halftone->window, &attrs);

  /* Make sure we have a valid buffer */
  update_buffer(halftone, &attrs);
  
  /* Make sure all dot attributes (spacing, width, height, etc) are correct */
  update_dot_attributes(halftone, &attrs);
  
  /* Move gravity points */
  for (i = 0; i < halftone->gravity_point_count; i++)
  {
    halftone->gravity_point_x_inc[i] = 
      (halftone->gravity_point_x[i] >= 1 || halftone->gravity_point_x[i] <= 0 ?
       -halftone->gravity_point_x_inc[i] : 
       halftone->gravity_point_x_inc[i]);
    halftone->gravity_point_y_inc[i] = 
      (halftone->gravity_point_y[i] >= 1 || halftone->gravity_point_y[i] <= 0 ?
       -halftone->gravity_point_y_inc[i] : 
       halftone->gravity_point_y_inc[i]);

    halftone->gravity_point_x[i] += halftone->gravity_point_x_inc[i];
    halftone->gravity_point_y[i] += halftone->gravity_point_y_inc[i];
  }

  /* Update gravity in each dot .*/
  for (x = 0; x < halftone->dots_width; x++)
    for (y = 0; y < halftone->dots_height; y++)
    {
      double gravity = calculate_gravity(halftone, x, y);

      halftone->dots[x + y * halftone->dots_width] = (gravity > 1 ? 1 : (gravity < 0 ? 0 : gravity));
    }
}


static unsigned long
halftone_draw (Display *dpy, Window window, void *closure)
{
  halftone_screen *halftone = (halftone_screen *) closure;

  repaint_halftone(halftone);
  update_halftone(halftone);

  return halftone->delay;
}


static void
halftone_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
halftone_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
halftone_free (Display *dpy, Window window, void *closure)
{
  halftone_screen *halftone = (halftone_screen *) closure;
  free (halftone);
}


static const char *halftone_defaults [] = {
  ".background:		Black",
  "*delay:		10000",
  "*count:		10",
  "*minMass:		0.001",
  "*maxMass:		0.02",
  "*minSpeed:		0.001",
  "*maxSpeed:		0.02",
  "*spacing:		14",
  "*sizeFactor:		1.5",
  "*colors:		200",
  "*cycleSpeed:		10",
#ifdef USE_IPHONE
  "*ignoreRotation:     True",
#endif
  0
};

static XrmOptionDescRec halftone_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-count",		".count",	XrmoptionSepArg, 0 },
  { "-minmass",		".minMass",	XrmoptionSepArg, 0 },
  { "-maxmass",		".maxMass",	XrmoptionSepArg, 0 },
  { "-minspeed",	".minSpeed",	XrmoptionSepArg, 0 },
  { "-maxspeed",	".maxSpeed",	XrmoptionSepArg, 0 },
  { "-spacing",		".spacing",	XrmoptionSepArg, 0 },
  { "-sizefactor",	".sizeFactor",	XrmoptionSepArg, 0 },
  { "-colors",		".colors",	XrmoptionSepArg, 0 },
  { "-cycle-speed",	".cycleSpeed",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Halftone", halftone)
