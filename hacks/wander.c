/* wander, by Rick Campbell <rick@campbellcentral.org>, 19 December 1998.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include <stdio.h>

#include "screenhack.h"
#include "colors.h"
#include "erase.h"

#define MAXIMUM_COLOR_COUNT (256)

struct state {
  Display *dpy;
  Window window;

   unsigned int advance;
   Bool         circles;
   Colormap     color_map;
   int          color_count;
   int          color_index;
   XColor       colors      [MAXIMUM_COLOR_COUNT];
   GC           context;
   unsigned int density;
   int          depth;
   int          height;
   unsigned int length;
   unsigned int reset;
   Bool reset_p;
   unsigned int size;
   int          width;
   int delay;

  int x, y, last_x, last_y, width_1, height_1, length_limit, reset_limit;
  unsigned long color;
  Pixmap pixmap;

  eraser_state *eraser;
};


static void *
wander_init (Display *dpy, Window window)
{
    struct state *st = (struct state *) calloc (1, sizeof(*st));
    XGCValues values;
    XWindowAttributes attributes;

    st->dpy = dpy;
    st->window = window;
    st->delay = get_integer_resource (st->dpy, "delay", "Integer");

    XClearWindow (st->dpy, st->window);
    XGetWindowAttributes (st->dpy, st->window, &attributes);
    st->width = attributes.width;
    st->height = attributes.height;
    st->depth = attributes.depth;
    st->color_map = attributes.colormap;
    if (st->color_count)
    {
        free_colors (attributes.screen, st->color_map,
                     st->colors, st->color_count);
        st->color_count = 0;
    }
    st->context = XCreateGC (st->dpy, st->window, 0, &values);
    st->color_count = MAXIMUM_COLOR_COUNT;
    make_color_loop (attributes.screen, attributes.visual, st->color_map,
                    0,   1, 1,
                    120, 1, 1,
                    240, 1, 1,
                    st->colors, &st->color_count, True, False);
    if (st->color_count <= 0)
    {
        st->color_count = 2;
        st->colors [0].red = st->colors [0].green = st->colors [0].blue = 0;
        st->colors [1].red = st->colors [1].green = st->colors [1].blue = 0xFFFF;
        XAllocColor (st->dpy, st->color_map, &st->colors [0]);
        XAllocColor (st->dpy, st->color_map, &st->colors [1]);
    }
    st->color_index = NRAND (st->color_count);
    
    st->advance = get_integer_resource (st->dpy, "advance", "Integer");
    st->density = get_integer_resource (st->dpy, "density", "Integer");
    if (st->density < 1) st->density = 1;
    st->reset = get_integer_resource (st->dpy, "reset", "Integer");
    if (st->reset < 100) st->reset = 100;
    st->circles = get_boolean_resource (st->dpy, "circles", "Boolean");
    st->size = get_integer_resource (st->dpy, "size", "Integer");
    if (st->size < 1) st->size = 1;
    if (st->width > 2560 || st->height > 2560)
      st->size *= 3;  /* Retina displays */
    st->width = st->width / st->size;
    st->height = st->height / st->size;
    st->length = get_integer_resource (st->dpy, "length", "Integer");
    if (st->length < 1) st->length = 1;
    XSetForeground (st->dpy, st->context, st->colors [st->color_index].pixel);


    st->x = NRAND (st->width);
    st->y = NRAND (st->height);
    st->last_x = st->x;
    st->last_y = st->y;
    st->width_1 = st->width - 1;
    st->height_1 = st->height - 1;
    st->length_limit = st->length;
    st->reset_limit = st->reset;
    st->color_index = NRAND (st->color_count);
    st->color = st->colors [NRAND (st->color_count)].pixel;
    st->pixmap = XCreatePixmap (st->dpy, window, st->size,
                            st->size, st->depth);

    XSetForeground (st->dpy, st->context,
		    BlackPixel (st->dpy, DefaultScreen (st->dpy)));
    XFillRectangle (st->dpy, st->pixmap, st->context, 0, 0,
		    st->width * st->size, st->height * st->size);
    XSetForeground (st->dpy, st->context, st->color);
    XFillArc (st->dpy, st->pixmap, st->context, 0, 0, st->size, st->size, 0, 360*64);

    return st;
}


static unsigned long
wander_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int i;

  if (st->eraser) {
    st->eraser = erase_window (st->dpy, st->window, st->eraser);
    goto END;
  }

  for (i = 0; i < 2000; i++)
    {
      if (NRAND (st->density))
        {
          st->x = st->last_x;
          st->y = st->last_y;
        }
      else
        {
          st->last_x = st->x;
          st->last_y = st->y;
          st->x += st->width_1  + NRAND (3);
          while (st->x >= st->width)
            st->x -= st->width;
          st->y += st->height_1 + NRAND (3);
          while (st->y >= st->height)
            st->y -= st->height;
        }

      if (NRAND (st->length_limit) == 0)
        {
          if (st->advance == 0)
            {
              st->color_index = NRAND (st->color_count);
            }
          else
            {
              st->color_index = (st->color_index + st->advance) % st->color_count;
            }
          st->color = st->colors [st->color_index].pixel;
          XSetForeground (st->dpy, st->context, st->color);
          if (st->circles)
            {
              XFillArc (st->dpy, st->pixmap, st->context,
                        0, 0, st->size, st->size, 0, 360 * 64);
            }
        }

      if (st->reset_p || NRAND (st->reset_limit) == 0)
        {
          st->reset_p = 0;
          st->eraser = erase_window (st->dpy, st->window, st->eraser);
          st->color = st->colors [NRAND (st->color_count)].pixel;
          st->x = NRAND (st->width);
          st->y = NRAND (st->height);
          st->last_x = st->x;
          st->last_y = st->y;
          if (st->circles)
            {
              XFillArc (st->dpy, st->pixmap, st->context, 0, 0, st->size, st->size, 0, 360*64);
            }
        }

      if (st->size == 1)
        {
          XDrawPoint (st->dpy, st->window, st->context, st->x, st->y);
        }
      else
        {
          if (st->circles)
            {
              XCopyArea (st->dpy, st->pixmap, st->window, st->context, 0, 0, st->size, st->size,
                         st->x * st->size, st->y * st->size);
            }
          else
            {
              XFillRectangle (st->dpy, st->window, st->context, st->x * st->size, st->y * st->size,
                              st->size, st->size);
            }
        }
    }

 END:
  return st->delay;
}


static void
wander_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  st->width  = w / st->size;
  st->height = h / st->size;
  st->width_1  = st->width - 1;
  st->height_1 = st->height - 1;
}

static Bool
wander_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->reset_p = 1;
      return True;
    }
  return False;
}

static void
wander_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (st->dpy, st->context);
  if (st->eraser) eraser_free (st->eraser);
  free (st);
}

static const char *wander_defaults [] =
{
/*    ".lowrez:     true", */
    ".background: black",
    ".foreground: white",
    ".fpsSolid:	  true",
    ".advance:    1",
    ".density:    2",
    ".length:     25000",
    ".delay:      20000",
    ".reset:      2500000",
    ".circles:    False",
    ".size:       1",
#ifdef HAVE_MOBILE
  "*ignoreRotation: True",
#endif
    0
};

static XrmOptionDescRec wander_options [] =
{
    { "-advance", ".advance", XrmoptionSepArg, 0 },
    { "-circles", ".circles",   XrmoptionNoArg, "True" },
    { "-no-circles",".circles", XrmoptionNoArg, "False" },
    { "-density", ".density", XrmoptionSepArg, 0 },
    { "-length",  ".length",  XrmoptionSepArg, 0 },
    { "-delay",   ".delay",   XrmoptionSepArg, 0 },
    { "-reset",   ".reset",   XrmoptionSepArg, 0 },
    { "-size",    ".size",    XrmoptionSepArg, 0 },
    { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Wander", wander)
