/* xscreensaver, Copyright (c) 2005 Jamie Zawinski <jwz@jwz.org>
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
#include <X11/Xutil.h>

#define ALIVE   1
#define CHANGED 2

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
  unsigned long bg_color;
  int border_size;
  int spacing;
  int inc;

  Bool circles_p;
  Bool growing_p;
  Bool color_horiz_p;

  int box_count;
  int boxes_size;
  int nboxes;
  box *boxes;

  int ncolors;
  XColor *colors;
} state;


static void
reset_boxes (state *st)
{
  static Bool once = False;
  int mode = -1;

  st->nboxes = 0;
  st->growing_p = True;
  st->color_horiz_p = random() & 1;

  if (once)
    free_colors (st->dpy, st->xgwa.colormap, st->colors, st->ncolors);

  if (!once)
    {
      char *s = get_string_resource ("mode", "Mode");
      if (!s || !*s || !strcasecmp (s, "random"))
        mode = -1;
      else if (!strcasecmp (s, "squares") || !strcasecmp (s, "square"))
        mode = 0;
      else if (!strcasecmp (s, "circles") || !strcasecmp (s, "circle"))
        mode = 1;
      else
        {
          fprintf (stderr,
                   "%s: mode must be random, squares, or circles, not '%s'\n",
                   progname, s);
          exit (1);
        }
    }

  if (mode == -1)
    st->circles_p = random() & 1;
  else
    st->circles_p = (mode == 1);

  once = True;

  st->ncolors = get_integer_resource ("colors", "Colors");  /* re-get this */
  make_smooth_colormap (st->dpy, st->xgwa.visual, st->xgwa.colormap,
                        st->colors, &st->ncolors, True, 0, False);
  XClearWindow (st->dpy, st->window);
}


state *
init_boxes (Display *dpy, Window window)
{
  XGCValues gcv;
  state *st = (state *) calloc (1, sizeof (*st));

  st->dpy = dpy;
  st->window = window;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  XSelectInput (dpy, window, st->xgwa.your_event_mask | ExposureMask);

  st->ncolors = get_integer_resource ("colors", "Colors");
  if (st->ncolors < 1) st->ncolors = 1;
  st->colors = (XColor *) malloc (sizeof(XColor) * st->ncolors);

  st->inc = get_integer_resource ("growBy", "GrowBy");
  st->spacing = get_integer_resource ("spacing", "Spacing");
  st->border_size = get_integer_resource ("borderSize", "BorderSize");
  st->bg_color = get_pixel_resource ("background", "Background",
                                     st->dpy, st->xgwa.colormap);
  if (st->inc < 1) st->inc = 1;
  if (st->border_size < 0) st->border_size = 0;

  gcv.line_width = st->border_size;
  gcv.background = st->bg_color;
  st->gc = XCreateGC (st->dpy, st->window, GCBackground|GCLineWidth, &gcv);

  st->box_count = get_integer_resource ("boxCount", "BoxCount");
  if (st->box_count < 1) st->box_count = 1;

  st->nboxes = 0;
  st->boxes_size = st->box_count * 2;
  st->boxes = (box *) calloc (st->boxes_size, sizeof(*st->boxes));

  reset_boxes (st);

  return st;
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


static void
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
      a->flags |= CHANGED;

      for (i = 0; i < 10000; i++)
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

          XSync (st->dpy, False);
          sleep (1);

          break;
        }

      /* Pick colors for this box */
      {
        int n = (st->color_horiz_p
                 ? (a->x * st->ncolors / st->xgwa.width)
                 : (a->y * st->ncolors / st->xgwa.height));
        a->fill_color   = st->colors [n % st->ncolors].pixel;
      }
    }
}


static void
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

  if (remaining == 0)
    reset_boxes (st);
}


static void
draw_boxes (state *st)
{
  int i;
  for (i = 0; i < st->nboxes; i++)
    {
      box *b = &st->boxes[i];

      if (!st->growing_p)
        {
          /* When shrinking, black out an area outside of the border
             before re-drawing the box.
           */
          XSetForeground (st->dpy, st->gc, st->bg_color);
          XSetLineAttributes (st->dpy, st->gc,
                              (st->inc + st->border_size) * 2,
                              LineSolid, CapButt, JoinMiter);

          if (st->circles_p)
            XDrawArc (st->dpy, st->window, st->gc,
                      b->x, b->y,
                      (b->w > 0 ? b->w : 1),
                      (b->h > 0 ? b->h : 1),
                      0, 360*64);
          else
            XDrawRectangle (st->dpy, st->window, st->gc,
                            b->x, b->y,
                            (b->w > 0 ? b->w : 1),
                            (b->h > 0 ? b->h : 1));
          XSetLineAttributes (st->dpy, st->gc, st->border_size,
                              LineSolid, CapButt, JoinMiter);
        }

      if (b->w <= 0 || b->h <= 0) continue;
      if (! (b->flags & CHANGED)) continue;
      b->flags &= ~CHANGED;

      XSetForeground (st->dpy, st->gc, b->fill_color);

      if (st->circles_p)
        XFillArc (st->dpy, st->window, st->gc, b->x, b->y, b->w, b->h,
                  0, 360*64);
      else
        XFillRectangle (st->dpy, st->window, st->gc, b->x, b->y, b->w, b->h);

      if (st->border_size > 0)
        {
          unsigned long bd = st->colors [(b->fill_color + st->ncolors/2)
                                         % st->ncolors].pixel;
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

static void
handle_events (state *st)
{
  XSync (st->dpy, False);
  while (XPending (st->dpy))
    {
      XEvent event;
      XNextEvent (st->dpy, &event);
      if (event.xany.type == ConfigureNotify ||
          event.xany.type == Expose)
        reshape_boxes (st);
      else if (event.xany.type == ButtonPress)
        st->growing_p = !st->growing_p;

      screenhack_handle_event (st->dpy, &event);
    }
}



char *progclass = "BoxFit";

char *defaults [] = {
  ".background:		   black",
  "*delay:		   20000",
  "*mode:		   random",
  "*colors:		   64",
  "*boxCount:		   50",
  "*growBy:		   1",
  "*spacing:		   1",
  "*borderSize:		   1",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-colors",		".colors",		XrmoptionSepArg, 0 },
  { "-count",		".boxCount",		XrmoptionSepArg, 0 },
  { "-growby",		".growBy",		XrmoptionSepArg, 0 },
  { "-spacing",		".spacing",		XrmoptionSepArg, 0 },
  { "-border",		".borderSize",		XrmoptionSepArg, 0 },
  { "-circles",		".mode",		XrmoptionNoArg, "circles" },
  { "-squares",		".mode",		XrmoptionNoArg, "squares" },
  { "-random",		".mode",		XrmoptionNoArg, "random"  },
  { 0, 0, 0, 0 }
};


void
screenhack (Display *dpy, Window window)
{
  state *st = init_boxes (dpy, window);
  int delay = get_integer_resource ("delay", "Integer");
  reshape_boxes (st);
  while (1)
    {
      if (st->growing_p)
        grow_boxes (st);
      else
        shrink_boxes (st);

      draw_boxes (st);
      handle_events (st);
      if (delay) usleep (delay);
    }
}
