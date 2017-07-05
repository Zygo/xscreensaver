/* xscreensaver, Copyright (c) 2005-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Boxfit -- fills space with a gradient of growing boxes or circles.
 *
 * Written by jwz, 21-Feb-2005.
 *
 * Inspired by http://www.levitated.net/daily/levBoxFitting.html
 */

#include "screenhack.h"
#include <stdio.h>
#include "xpm-pixmap.h"

#define ALIVE   1
#define CHANGED 2
#define UNDEAD  4

typedef struct {
  unsigned long fill_color;
  short x, y, w, h;
  char flags;
} box;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  GC gc;
  unsigned long fg_color, bg_color;
  int border_size;
  int spacing;
  int inc;

  int mode;
  Bool circles_p;
  Bool growing_p;
  Bool color_horiz_p;

  int box_count;
  int boxes_size;
  int nboxes;
  box *boxes;

  XImage *image;
  int ncolors;
  XColor *colors;
  int delay;
  int countdown;

  Bool done_once;
  async_load_state *img_loader;
  Pixmap loading_pixmap;

} state;


static void
reset_boxes (state *st)
{
  st->nboxes = 0;
  st->growing_p = True;
  st->color_horiz_p = random() & 1;

  if (st->done_once && st->colors)
    free_colors (st->xgwa.screen, st->xgwa.colormap, st->colors, st->ncolors);

  if (!st->done_once)
    {
      char *s = get_string_resource (st->dpy, "mode", "Mode");
      if (!s || !*s || !strcasecmp (s, "random"))
        st->mode = -1;
      else if (!strcasecmp (s, "squares") || !strcasecmp (s, "square"))
        st->mode = 0;
      else if (!strcasecmp (s, "circles") || !strcasecmp (s, "circle"))
        st->mode = 1;
      else
        {
          fprintf (stderr,
                   "%s: mode must be random, squares, or circles, not '%s'\n",
                   progname, s);
          exit (1);
        }
    }

  if (st->mode == -1)
    st->circles_p = random() & 1;
  else
    st->circles_p = (st->mode == 1);

  st->done_once = True;

  if (st->image || get_boolean_resource (st->dpy, "grab", "Boolean"))
    {
      if (st->image) XDestroyImage (st->image);
      st->image = 0;

      if (st->loading_pixmap) abort();
      if (st->img_loader) abort();
      if (!get_boolean_resource (st->dpy, "peek", "Boolean"))
        st->loading_pixmap = XCreatePixmap (st->dpy, st->window,
                                            st->xgwa.width, st->xgwa.height,
                                            st->xgwa.depth);

      XClearWindow (st->dpy, st->window);
      st->img_loader = load_image_async_simple (0, st->xgwa.screen, 
                                                st->window,
                                                (st->loading_pixmap
                                                 ? st->loading_pixmap
                                                 : st->window), 
                                                0, 0);
    }
  else
    {
      st->ncolors = get_integer_resource (st->dpy, "colors", "Colors");  /* re-get */
      if (st->ncolors < 1) st->ncolors = 1;
      make_smooth_colormap (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                            st->colors, &st->ncolors, True, 0, False);
      if (st->ncolors < 1) abort();
      XClearWindow (st->dpy, st->window);
    }
}


static void
reshape_boxes (state *st)
{
  int i;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  for (i = 0; i < st->nboxes; i++)
    {
      box *b = &st->boxes[i];
      b->flags |= CHANGED;
    }
}

static void *
boxfit_init (Display *dpy, Window window)
{
  XGCValues gcv;
  state *st = (state *) calloc (1, sizeof (*st));

  st->dpy = dpy;
  st->window = window;
  st->delay = get_integer_resource (dpy, "delay", "Integer");

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
/*  XSelectInput (dpy, window, st->xgwa.your_event_mask | ExposureMask);*/

  if (! get_boolean_resource (dpy, "grab", "Boolean"))
    {
      st->ncolors = get_integer_resource (dpy, "colors", "Colors");
      if (st->ncolors < 1) st->ncolors = 1;
      st->colors = (XColor *) malloc (sizeof(XColor) * st->ncolors);
    }

  st->inc = get_integer_resource (dpy, "growBy", "GrowBy");
  st->spacing = get_integer_resource (dpy, "spacing", "Spacing");
  st->border_size = get_integer_resource (dpy, "borderSize", "BorderSize");
  st->fg_color = get_pixel_resource (st->dpy, st->xgwa.colormap, 
                                     "foreground", "Foreground");
  st->bg_color = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                     "background", "Background");
  if (st->inc < 1) st->inc = 1;
  if (st->border_size < 0) st->border_size = 0;

  gcv.line_width = st->border_size;
  gcv.background = st->bg_color;
  st->gc = XCreateGC (st->dpy, st->window, GCBackground|GCLineWidth, &gcv);

  st->box_count = get_integer_resource (dpy, "boxCount", "BoxCount");
  if (st->box_count < 1) st->box_count = 1;

  st->nboxes = 0;
  st->boxes_size = st->box_count * 2;
  st->boxes = (box *) calloc (st->boxes_size, sizeof(*st->boxes));

  reset_boxes (st);

  reshape_boxes (st);
  return st;
}



static Bool
boxes_overlap_p (box *a, box *b, int pad)
{
  /* Two rectangles overlap if the max of the tops is less than the
     min of the bottoms and the max of the lefts is less than the min
     of the rights.
   */
# undef MAX
# undef MIN
# define MAX(A,B) ((A)>(B)?(A):(B))
# define MIN(A,B) ((A)<(B)?(A):(B))

  int maxleft  = MAX(a->x - pad, b->x);
  int maxtop   = MAX(a->y - pad, b->y);
  int minright = MIN(a->x + a->w + pad + pad - 1, b->x + b->w);
  int minbot   = MIN(a->y + a->h + pad + pad - 1, b->y + b->h);
  return (maxtop < minbot && maxleft < minright);
}


static Bool
circles_overlap_p (box *a, box *b, int pad)
{
  int ar = a->w/2;	/* radius */
  int br = b->w/2;
  int ax = a->x + ar;	/* center */
  int ay = a->y + ar;
  int bx = b->x + br;
  int by = b->y + br;
  int d2 = (((bx - ax) * (bx - ax)) +	/* distance between centers squared */
            ((by - ay) * (by - ay)));
  int r2 = ((ar + br + pad) *		/* sum of radii squared */
            (ar + br + pad));
  return (d2 < r2);
}


static Bool
box_collides_p (state *st, box *a, int pad)
{
  int i;

  /* collide with wall */
  if (a->x - pad < 0 ||
      a->y - pad < 0 ||
      a->x + a->w + pad + pad >= st->xgwa.width ||
      a->y + a->h + pad + pad >= st->xgwa.height)
    return True;

  /* collide with another box */
  for (i = 0; i < st->nboxes; i++)
    {
      box *b = &st->boxes[i];
      if (a != b &&
          (st->circles_p
           ? circles_overlap_p (a, b, pad)
           : boxes_overlap_p (a, b, pad)))
        return True;
    }

  return False;
}


static unsigned int
grow_boxes (state *st)
{
  int inc2 = st->inc + st->spacing + st->border_size;
  int i;
  int live_count = 0;

  /* check box collisions, and grow if none.
   */
  for (i = 0; i < st->nboxes; i++)
    {
      box *a = &st->boxes[i];
      if (!(a->flags & ALIVE)) continue;

      if (box_collides_p (st, a, inc2))
        {
          a->flags &= ~ALIVE;
          continue;
        }
      
      live_count++;
      a->x -= st->inc;
      a->y -= st->inc;
      a->w += st->inc + st->inc;
      a->h += st->inc + st->inc;
      a->flags |= CHANGED;
    }

  /* Add more boxes.
   */
  while (live_count < st->box_count)
    {
      box *a;
      st->nboxes++;
      if (st->boxes_size <= st->nboxes)
        {
          st->boxes_size = (st->boxes_size * 1.2) + st->nboxes;
          st->boxes = (box *)
            realloc (st->boxes, st->boxes_size * sizeof(*st->boxes));
          if (! st->boxes)
            {
              fprintf (stderr, "%s: out of memory (%d boxes)\n",
                       progname, st->boxes_size);
              exit (1);
            }
        }

      a = &st->boxes[st->nboxes-1];
      a->flags = CHANGED;

      for (i = 0; i < 100; i++)
        {
          a->x = inc2 + (random() % (st->xgwa.width  - inc2));
          a->y = inc2 + (random() % (st->xgwa.height - inc2));
          a->w = 0;
          a->h = 0;

          if (! box_collides_p (st, a, inc2))
            {
              a->flags |= ALIVE;
              live_count++;
              break;
            }
        }

      if (! (a->flags & ALIVE) ||	/* too many retries; */
          st->nboxes > 65535)		/* that's about 1MB of box structs. */
        {
          st->nboxes--;			/* go into "fade out" mode now. */
          st->growing_p = False;
          return 2000000; /* make customizable... */
        }

      /* Pick colors for this box */
      if (st->image)
        {
          int w = st->image->width;
          int h = st->image->height;
          a->fill_color = XGetPixel (st->image, a->x % w, a->y % h);
        }
      else
        {
          int n = (st->color_horiz_p
                   ? (a->x * st->ncolors / st->xgwa.width)
                   : (a->y * st->ncolors / st->xgwa.height));
          a->fill_color   = st->colors [n % st->ncolors].pixel;
        }
    }

  return st->delay;
}


static unsigned int
shrink_boxes (state *st)
{
  int i;
  int remaining = 0;

  for (i = 0; i < st->nboxes; i++)
    {
      box *a = &st->boxes[i];

      if (a->w <= 0 || a->h <= 0) continue;

      a->x += st->inc;
      a->y += st->inc;
      a->w -= st->inc + st->inc;
      a->h -= st->inc + st->inc;
      a->flags |= CHANGED;
      if (a->w < 0) a->w = 0;
      if (a->h < 0) a->h = 0;

      if (a->w > 0 && a->h > 0)
        remaining++;
    }

  if (remaining == 0) {
    reset_boxes (st);
    return 1000000;
  } else {
    return st->delay;
  }
}


static void
draw_boxes (state *st)
{
  int i;
  for (i = 0; i < st->nboxes; i++)
    {
      box *b = &st->boxes[i];

      if (b->flags & UNDEAD) continue;
      if (! (b->flags & CHANGED)) continue;
      b->flags &= ~CHANGED;

      if (!st->growing_p)
        {
          /* When shrinking, black out an area outside of the border
             before re-drawing the box.
           */
          int margin = st->inc + st->border_size;

          XSetForeground (st->dpy, st->gc, st->bg_color);
          if (st->circles_p)
            XFillArc (st->dpy, st->window, st->gc,
                      b->x - margin, b->y - margin,
                      b->w + (margin*2), b->h + (margin*2),
                      0, 360*64);
          else
            XFillRectangle (st->dpy, st->window, st->gc,
                      b->x - margin, b->y - margin,
                      b->w + (margin*2), b->h + (margin*2));

          if (b->w <= 0 || b->h <= 0)
            b->flags |= UNDEAD;   /* really very dead now */
        }

      if (b->w <= 0 || b->h <= 0) continue;

      XSetForeground (st->dpy, st->gc, b->fill_color);

      if (st->circles_p)
        XFillArc (st->dpy, st->window, st->gc, b->x, b->y, b->w, b->h,
                  0, 360*64);
      else
        XFillRectangle (st->dpy, st->window, st->gc, b->x, b->y, b->w, b->h);

      if (st->border_size > 0)
        {
          unsigned int bd = (st->image
                             ? st->fg_color
                             : st->colors [(b->fill_color + st->ncolors/2)
                                           % st->ncolors].pixel);
          XSetForeground (st->dpy, st->gc, bd);
          if (st->circles_p)
            XDrawArc (st->dpy, st->window, st->gc, b->x, b->y, b->w, b->h,
                      0, 360*64);
          else
            XDrawRectangle (st->dpy, st->window, st->gc,
                            b->x, b->y, b->w, b->h);
        }
    }
}


static unsigned long
boxfit_draw (Display *dpy, Window window, void *closure)
{
  state *st = (state *) closure;
  int delay;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);
      if (! st->img_loader)  /* just finished */
        {
          st->image = XGetImage (st->dpy,
                                 (st->loading_pixmap ? st->loading_pixmap :
                                  st->window),
                                 0, 0,
                                 st->xgwa.width, st->xgwa.height, ~0L, 
                                 ZPixmap);
          if (st->loading_pixmap) XFreePixmap (st->dpy, st->loading_pixmap);
          XSetWindowBackground (st->dpy, st->window, st->bg_color);
          if (st->loading_pixmap)
            XClearWindow (st->dpy, st->window);
          else
            st->countdown = 2000000;
          st->loading_pixmap = 0;
        }
      return st->delay;
    }

  if (st->countdown > 0)
    {
      st->countdown -= st->delay;
      if (st->countdown <= 0)
        {
          st->countdown = 0;
          XClearWindow (st->dpy, st->window);
        }
      return st->delay;
    }

  if (st->growing_p) {
    draw_boxes (st);
    delay = grow_boxes (st);
  } else {
    delay = shrink_boxes (st);
    draw_boxes (st);
  }
  return delay;
}

static void
boxfit_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  state *st = (state *) closure;
  reshape_boxes (st);
}

static Bool
boxfit_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  state *st = (state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      st->growing_p = !st->growing_p;
      return True;
    }
  return False;
}

static void
boxfit_free (Display *dpy, Window window, void *closure)
{
}


static const char *boxfit_defaults [] = {
  ".background:		   black",
  ".foreground:		   #444444",
  "*fpsSolid:		   true",
  "*delay:		   20000",
  "*mode:		   random",
  "*colors:		   64",
  "*boxCount:		   50",
  "*growBy:		   1",
  "*spacing:		   1",
  "*borderSize:		   1",
  "*grab:		   False",
  "*peek:		   False",
  "*grabDesktopImages:     False",   /* HAVE_JWXYZ */
  "*chooseRandomImages:    True",    /* HAVE_JWXYZ */
#ifdef HAVE_MOBILE
  "*ignoreRotation:	   True",
  "*rotateImages:          True",
#endif
  0
};

static XrmOptionDescRec boxfit_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-colors",		".colors",		XrmoptionSepArg, 0 },
  { "-count",		".boxCount",		XrmoptionSepArg, 0 },
  { "-growby",		".growBy",		XrmoptionSepArg, 0 },
  { "-spacing",		".spacing",		XrmoptionSepArg, 0 },
  { "-border",		".borderSize",		XrmoptionSepArg, 0 },
  { "-mode",		".mode",		XrmoptionSepArg, 0 },
  { "-circles",		".mode",		XrmoptionNoArg, "circles" },
  { "-squares",		".mode",		XrmoptionNoArg, "squares" },
  { "-random",		".mode",		XrmoptionNoArg, "random"  },
  { "-grab",	  	".grab",		XrmoptionNoArg, "True"    },
  { "-no-grab",  	".grab",		XrmoptionNoArg, "False"   },
  { "-peek",	  	".peek",		XrmoptionNoArg, "True"    },
  { "-no-peek",  	".peek",		XrmoptionNoArg, "False"   },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("BoxFit", boxfit)
