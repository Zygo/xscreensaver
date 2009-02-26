/* xscreensaver, Copyright (c) 1997-2008 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*
  TODO:

    =  Rather than just flickering the pieces before swapping them,
       show them lifting up and moving to their new positions.
       The path on which they move shouldn't be a straight line;
       try to avoid having them cross each other by moving them in
       oppositely-positioned arcs.

    =  Rotate the pieces as well, so that we can swap the corner
       and edge pieces with each other.

    =  Have it drop all pieces to the "floor" then pick them up to
       reassemble the picture.

    =  As a joke, maybe sometimes have one piece that doesn't fit?
       Or lose a piece?
 */

#include "screenhack.h"
#include "spline.h"

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#define CENTER	  0
#define NORTH	  1
#define NORTHEAST 2
#define EAST	  3
#define SOUTHEAST 4
#define SOUTH	  5
#define SOUTHWEST 6
#define WEST	  7
#define NORTHWEST 8

struct piece {
  int width, height;
  int x, y;
  Pixmap pixmap;
};

struct set {
  struct piece pieces[9];
};

#define PIECE_A_HOLLOW 0
#define PIECE_A_FILLED 1
#define PIECE_B_HOLLOW 2
#define PIECE_B_FILLED 3

struct swap_state {
  int flashing;
  int x1, y1, x2, y2;
  Bool draw_p;
};

struct state {
  Display *dpy;
  Window window;

  struct set all_pieces[4];

  int piece_width, piece_height;
  int width, height;
  int x_border, y_border;
  Pixmap source;
  GC gc;
  int fg, bg;
  int border_width;
  XPoint *state;
  int delay, delay2;

  int jigstate;

  struct swap_state swap;
  int clearing;

  async_load_state *img_loader;
};


/* Returns a spline describing one edge of a puzzle piece of the given length.
 */
static spline *
make_puzzle_curve (int pixels)
{
  double x0 = 0.0000, y0 =  0.0000;
  double x1 = 0.3333, y1 =  0.1000;
  double x2 = 0.4333, y2 =  0.0333;
  double x3 = 0.4666, y3 = -0.0666;
  double x4 = 0.3333, y4 = -0.1666;
  double x5 = 0.3666, y5 = -0.2900;
  double x6 = 0.5000, y6 = -0.3333;

  spline *s = make_spline(20);
  s->n_controls = 0;

# define PT(x,y) \
    s->control_x[s->n_controls] = pixels * (x); \
    s->control_y[s->n_controls] = pixels * (y); \
    s->n_controls++
  PT (  x0, y0);
  PT (  x1, y1);
  PT (  x2, y2);
  PT (  x3, y3);
  PT (  x4, y4);
  PT (  x5, y5);
  PT (  x6, y6);
  PT (1-x5, y5);
  PT (1-x4, y4);
  PT (1-x3, y3);
  PT (1-x2, y2);
  PT (1-x1, y1);
  PT (1-x0, y0);
# undef PT

  compute_spline (s);
  return s;
}


/* Draws a puzzle piece.  The top/right/bottom/left_type args
   indicate the direction the tabs point: 1 for out, -1 for in, 0 for flat.
 */
static void
draw_puzzle_shape (Display *dpy, Drawable d, GC gc,
                   int x, int y, int size, int bw,
                   int top_type, int right_type,
                   int bottom_type, int left_type,
                   Bool fill_p)
{
  spline *s = make_puzzle_curve (size);
  XPoint *pts = (XPoint *) malloc (s->n_points * 4 * sizeof(*pts));
  int i, o;

  /* The border is twice as wide for "flat" edges, otherwise it looks funny. */
  if (fill_p) 
    bw = 0;
  else
    bw /= 2;

  o = 0;
  if (top_type == 0) {
    pts[o].x = x;        pts[o].y = y + bw; o++;
    pts[o].x = x + size; pts[o].y = y + bw; o++;
  } else {
    for (i = 0; i < s->n_points; i++) {
      pts[o].x = x + s->points[i].x;
      pts[o].y = y + s->points[i].y * top_type;
      o++;
    }
  }

  if (right_type == 0) {
    pts[o-1].x -= bw;
    pts[o].x = x + size - bw; pts[o].y = y + size; o++;
  } else {
    for (i = 1; i < s->n_points; i++) {
      pts[o].x = x + size + s->points[i].y * (-right_type);
      pts[o].y = y        + s->points[i].x;
      o++;
    }
  }

  if (bottom_type == 0) {
    pts[o-1].y -= bw;
    pts[o].x = x; pts[o].y = y + size - bw; o++;
  } else {
    for (i = 1; i < s->n_points; i++) {
      pts[o].x = x        + s->points[s->n_points-i-1].x;
      pts[o].y = y + size + s->points[s->n_points-i-1].y * (-bottom_type);
      o++;
    }
  }

  if (left_type == 0) {
    pts[o-1].x += bw;
    pts[o].x = x + bw; pts[o].y = y; o++;
  } else {
    for (i = 1; i < s->n_points; i++) {
      pts[o].x = x + s->points[s->n_points-i-1].y * left_type;
      pts[o].y = y + s->points[s->n_points-i-1].x;
      o++;
    }
  }

  free_spline (s);

  if (fill_p)
    XFillPolygon (dpy, d, gc, pts, o, Complex, CoordModeOrigin);
  else
    XDrawLines (dpy, d, gc, pts, o, CoordModeOrigin);

  free (pts);
}


/* Creates two pixmaps for a puzzle piece:
   - The first is a solid bit-mask with 1 for each pixel inside the piece;
   - The second is an outline of the piece, where all drawn pixels are
     contained within the mask.

   The top/right/bottom/left_type args indicate the direction the
   tabs point: 1 for out, -1 for in, 0 for flat.

   Size is how big the piece should be, from origin to origin.

   Returned x/y is the origin within the pixmaps.
 */
static void
make_puzzle_pixmap_pair (Display *dpy, Drawable d, int size, int bw,
                         int top_type, int right_type, 
                         int bottom_type, int left_type,
                         int *x_ret, int *y_ret,
                         Pixmap *mask_ret, Pixmap *outline_ret)
{
  int w = (size ? size * 3 : 2);
  int h = w;
  int x = size;
  int y = size;
  Pixmap p0 = XCreatePixmap (dpy, d, w, h, 1);
  Pixmap p1 = XCreatePixmap (dpy, d, w, h, 1);
  XGCValues gcv;
  GC gc;
  gcv.foreground = 0;
  gcv.background = 0;
  gc = XCreateGC (dpy, p0, GCForeground|GCBackground, &gcv);
  XFillRectangle (dpy, p0, gc, 0, 0, w, h);
  XFillRectangle (dpy, p1, gc, 0, 0, w, h);
  XSetForeground (dpy, gc, 0);

# ifdef HAVE_COCOA
  jwxyz_XSetAlphaAllowed (dpy, gc, False);
# endif

  /* To ensure that each pixel is drawn only once, we render the piece
     such that it "owns" the left and top edges, but not the right and
     bottom edges.

         - - +      "#" is this piece.
         - # +      It overlaps "-" and is overlapped by "+".
         - + +

     To accomplish this, we clear to black, draw "#" in white,
     then draw "+" in black.
   */

  /* Center square */
  XSetForeground (dpy, gc, 1);
  draw_puzzle_shape (dpy, p0, gc, x, y, size, bw,
                     top_type, right_type, bottom_type, left_type,
                     True);

  /* Top right square */
  XSetForeground (dpy, gc, 0);
  draw_puzzle_shape (dpy, p0, gc, x + size, y - size, size, bw,
                     0, 0, -top_type, -left_type,
                     True);

  /* Center right square */
  draw_puzzle_shape (dpy, p0, gc, x + size, y, size, bw,
                     0, 0, 0, -right_type,
                     True);

  /* Bottom center square */
  draw_puzzle_shape (dpy, p0, gc, x, y + size, size, bw,
                     -bottom_type, 0, 0, 0,
                     True);

  /* And Charles Nelson Reilly in the bottom right square */
  draw_puzzle_shape (dpy, p0, gc, x + size, y + size, size, bw,
                     -bottom_type, -right_type, 0, 0,
                     True);

  /* Done with p0 (the mask).
     To make p1 (the outline) draw an outlined piece through the mask.
   */
  if (bw < 0)
    {
      bw = size / 30;
      if (bw < 1) bw = 1;
    }

  if (bw > 0)
    {
      XSetForeground (dpy, gc, 1);
      XSetClipMask (dpy, gc, p0);
      XSetLineAttributes (dpy, gc, bw, LineSolid, CapButt, JoinRound);
      draw_puzzle_shape (dpy, p1, gc, x, y, size, bw,
                         top_type, right_type, bottom_type, left_type,
                         False);
    }

  XFreeGC (dpy, gc);
  *x_ret = x;
  *y_ret = x;
  *mask_ret = p0;
  *outline_ret = p1;
}


static void
make_puzzle_pixmaps (struct state *st)
{
  int i, j;

  int edges[9][4] = {
    { -1,  1, -1, 1 },	/* CENTER    */
    {  0,  1, -1, 1 },	/* NORTH     */
    {  0,  0, -1, 1 },	/* NORTHEAST */
    { -1,  0, -1, 1 },	/* EAST      */
    { -1,  0,  0, 1 },	/* SOUTHEAST */
    { -1,  1,  0, 1 },	/* SOUTH     */
    { -1,  1,  0, 0 },	/* SOUTHWEST */
    { -1,  1, -1, 0 },	/* WEST      */
    {  0,  1, -1, 0 },	/* NORTHWEST */
  };

  /* sometimes swap direction of horizontal edges */
  if (random() & 1)
    for (j = 0; j < countof(edges); j++) {
      edges[j][0] = -edges[j][0];
      edges[j][2] = -edges[j][2];
    }

  /* sometimes swap direction of vertical edges */
  if (random() & 1)
    for (j = 0; j < countof(edges); j++) {
      edges[j][1] = -edges[j][1];
      edges[j][3] = -edges[j][3];
    }

  for (j = 0; j < 9; j++) {
    for (i = 0; i < 2; i++) {
      int x, y;
      int top, right, bottom, left;
      Pixmap mask, outline;
      top    = edges[j][0];
      right  = edges[j][1];
      bottom = edges[j][2];
      left   = edges[j][3];
      if (i) {
        top    = -top; 
        right  = -right; 
        bottom = -bottom; 
        left   = -left;
      }
      make_puzzle_pixmap_pair (st->dpy, st->window, st->piece_width,
                               st->border_width,
                               top, right, bottom, left,
                               &x, &y, &mask, &outline);

      st->all_pieces[i*2].pieces[j].x = x;
      st->all_pieces[i*2].pieces[j].y = y;
      st->all_pieces[i*2].pieces[j].pixmap = outline;

      st->all_pieces[i*2+1].pieces[j].x = x;
      st->all_pieces[i*2+1].pieces[j].y = y;
      st->all_pieces[i*2+1].pieces[j].pixmap = mask;
    }
  }
}

static void
free_puzzle_pixmaps (struct state *st)
{
  int i, j;
  for (i = 0; i < countof(st->all_pieces); i++)
    for (j = 0; j < countof (st->all_pieces[i].pieces); j++)
      if (st->all_pieces[i].pieces[j].pixmap) {
        XFreePixmap (st->dpy, st->all_pieces[i].pieces[j].pixmap);
        st->all_pieces[i].pieces[j].pixmap = 0;
      }
}


static void
jigsaw_init_1 (struct state *st)
{
  XWindowAttributes xgwa;
  int x, y;
  XGCValues gcv;
  Colormap cmap;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);

  st->piece_width = 40 + (random() % 100);
  if (xgwa.width / st->piece_width < 4)
    st->piece_width = xgwa.width / 4;
  st->piece_height = st->piece_width;

  free_puzzle_pixmaps (st);
  make_puzzle_pixmaps (st);

  cmap = xgwa.colormap;
  st->width  = (st->piece_width ? xgwa.width  / st->piece_width : 0);
  st->height = (st->piece_height ? xgwa.height / st->piece_height : 0);
  st->x_border = (xgwa.width  - (st->width  * st->piece_width)) / 2;
  st->y_border = (xgwa.height - (st->height * st->piece_width)) / 2;

  if (st->width  < 4) st->width  = 4, st->x_border = 0;
  if (st->height < 4) st->height = 4, st->y_border = 0;

  if (st->state) free (st->state);
  st->state = (XPoint *) malloc (st->width * st->height * sizeof(*st->state));

  if (!st->gc)
    {
      XColor fgc, bgc;
      char *fgs = get_string_resource(st->dpy, "foreground", "Foreground");
      char *bgs = get_string_resource(st->dpy, "background", "Background");
      Bool fg_ok, bg_ok;

      st->gc = XCreateGC (st->dpy, st->window, 0, &gcv);

# ifdef HAVE_COCOA
      jwxyz_XSetAlphaAllowed (st->dpy, st->gc, False);
# endif

      if (!XParseColor (st->dpy, cmap, fgs, &fgc))
        XParseColor (st->dpy, cmap, "gray", &fgc);
      if (!XParseColor (st->dpy, cmap, bgs, &bgc))
        XParseColor (st->dpy, cmap, "black", &bgc);

      free (fgs);
      free (bgs);
      fgs = bgs = 0;

      fg_ok = XAllocColor (st->dpy, cmap, &fgc);
      bg_ok = XAllocColor (st->dpy, cmap, &bgc);

      /* If we weren't able to allocate the two colors we want from the
         colormap (which is likely if the screen has been grabbed on an
         8-bit SGI visual -- don't ask) then just go through the map
         and find the closest color to the ones we wanted, and use those
         pixels without actually allocating them.
      */
      if (fg_ok)
        st->fg = fgc.pixel;
      else
        st->fg = 0;

      if (bg_ok)
        st->bg = bgc.pixel;
      else
        st->bg = 1;

#ifndef HAVE_COCOA
      if (!fg_ok || bg_ok)
        {
          int i;
          unsigned long fgd = ~0;
          unsigned long bgd = ~0;
          int max = visual_cells (xgwa.screen, xgwa.visual);
          XColor *all = (XColor *) calloc(sizeof (*all), max);
          for (i = 0; i < max; i++)
            {
              all[i].flags = DoRed|DoGreen|DoBlue;
              all[i].pixel = i;
            }
          XQueryColors (st->dpy, cmap, all, max);
          for(i = 0; i < max; i++)
            {
              long rd, gd, bd;
              unsigned long d;
              if (!fg_ok)
                {
                  rd = (all[i].red   >> 8) - (fgc.red   >> 8);
                  gd = (all[i].green >> 8) - (fgc.green >> 8);
                  bd = (all[i].blue  >> 8) - (fgc.blue  >> 8);
                  if (rd < 0) rd = -rd;
                  if (gd < 0) gd = -gd;
                  if (bd < 0) bd = -bd;
                  d = (rd << 1) + (gd << 2) + bd;
                  if (d < fgd)
                    {
                      fgd = d;
                      st->fg = all[i].pixel;
                      if (d == 0)
                        fg_ok = True;
                    }
                }

              if (!bg_ok)
                {
                  rd = (all[i].red   >> 8) - (bgc.red   >> 8);
                  gd = (all[i].green >> 8) - (bgc.green >> 8);
                  bd = (all[i].blue  >> 8) - (bgc.blue  >> 8);
                  if (rd < 0) rd = -rd;
                  if (gd < 0) gd = -gd;
                  if (bd < 0) bd = -bd;
                  d = (rd << 1) + (gd << 2) + bd;
                  if (d < bgd)
                    {
                      bgd = d;
                      st->bg = all[i].pixel;
                      if (d == 0)
                        bg_ok = True;
                    }
                }

              if (fg_ok && bg_ok)
                break;
            }
          XFree(all);
        }
#endif /* HAVE_COCOA */
    }

  /* Reset the window's background color... */
  XSetWindowBackground (st->dpy, st->window, st->bg);
  XClearWindow(st->dpy, st->window);

  for (y = 0; y < st->height; y++)
    for (x = 0; x < st->width; x++)
      {
	st->state[y * st->width + x].x = x;
	st->state[y * st->width + x].y = y;
      }

  if (st->source)
    XFreePixmap (st->dpy, st->source);
  st->source = XCreatePixmap (st->dpy, st->window, xgwa.width, xgwa.height,
                              xgwa.depth);

  st->img_loader = load_image_async_simple (0, xgwa.screen, st->window,
                                            st->source, 0, 0);
}


static void
get_piece (struct state *st, 
           int x, int y, struct piece **hollow, struct piece **filled)
{
  int p;
  Bool which = (x & 1) == (y & 1);

  if      (x == 0           && y == 0)	          p = NORTHWEST;
  else if (x == st->width-1 && y == 0)	          p = NORTHEAST;
  else if (x == st->width-1 && y == st->height-1) p = SOUTHEAST;
  else if (x == 0           && y == st->height-1) p = SOUTHWEST;
  else if (y == 0)			          p = NORTH;
  else if (x == st->width-1)		          p = EAST;
  else if (y == st->height-1)		          p = SOUTH;
  else if (x == 0)			          p = WEST;
  else					          p = CENTER;

  if (hollow)
    *hollow = (which
	       ? &st->all_pieces[PIECE_A_HOLLOW].pieces[p]
	       : &st->all_pieces[PIECE_B_HOLLOW].pieces[p]);
  if (filled)
    *filled = (which
	       ? &st->all_pieces[PIECE_A_FILLED].pieces[p]
	       : &st->all_pieces[PIECE_B_FILLED].pieces[p]);
}


static void
draw_piece (struct state *st, int x, int y, int clear_p)
{
  struct piece *hollow, *filled;
  int from_x = st->state[y * st->width + x].x;
  int from_y = st->state[y * st->width + x].y;

  get_piece(st, x, y, &hollow, &filled);
	  
  XSetClipMask(st->dpy, st->gc, filled->pixmap);
  XSetClipOrigin(st->dpy, st->gc,
		 st->x_border + (x * st->piece_width) - filled->x - 1,
		 st->y_border + (y * st->piece_width) - filled->y - 1);

  if (clear_p)
    {
      XSetForeground(st->dpy, st->gc, st->bg);
      XFillRectangle(st->dpy, st->window, st->gc,
		     st->x_border + (x * st->piece_width)  -st->piece_width/2,
		     st->y_border + (y * st->piece_height) -st->piece_height/2,
		     st->piece_width*2, st->piece_height*2);
    }
  else
    XCopyArea(st->dpy, st->source, st->window, st->gc,
	      st->x_border + (from_x * st->piece_width)  - st->piece_width/2,
	      st->y_border + (from_y * st->piece_height) - st->piece_height/2,
	      st->piece_width*2, st->piece_height*2,
	      st->x_border + (x * st->piece_width)  - st->piece_width/2,
	      st->y_border + (y * st->piece_height) - st->piece_height/2);

  if (clear_p > 1)
    return;

  XSetForeground(st->dpy, st->gc, st->fg);
  XSetClipMask(st->dpy, st->gc, hollow->pixmap);
  XSetClipOrigin(st->dpy, st->gc,
		 st->x_border + (x * st->piece_width) - hollow->x - 1,
		 st->y_border + (y * st->piece_width) - hollow->y - 1);
  XFillRectangle(st->dpy, st->window, st->gc,
		 st->x_border + (x * st->piece_width)  - st->piece_width/2,
		 st->y_border + (y * st->piece_height) - st->piece_height/2,
		 st->piece_width*2, st->piece_height*2);
}


static int
animate_swap (struct state *st, struct swap_state *sw)
{
  XPoint swap;

  if (sw->flashing > 1)
    {
      draw_piece(st, sw->x1, sw->y1, sw->flashing & 1);
      draw_piece(st, sw->x2, sw->y2, sw->flashing & 1);
      sw->flashing--;
      return st->delay;
    }

  swap = st->state[sw->y1 * st->width + sw->x1];
  st->state[sw->y1 * st->width + sw->x1] =
    st->state[sw->y2 * st->width + sw->x2];
  st->state[sw->y2 * st->width + sw->x2] = swap;
  
  if (sw->draw_p)
    {
      draw_piece(st, sw->x1, sw->y1, 0);
      draw_piece(st, sw->x2, sw->y2, 0);
      sw->flashing = 0;
    }

  return 0;
}


static int
swap_pieces (struct state *st,
             int src_x, int src_y, int dst_x, int dst_y,
             Bool draw_p)
{
  struct swap_state *sw = &st->swap;
  
  sw->x1 = src_x;
  sw->y1 = src_y;
  sw->x2 = dst_x;
  sw->y2 = dst_y;
  sw->draw_p = draw_p;

  /* if animating, plan to flash the pieces on and off a few times */
  sw->flashing = sw->draw_p ? 7 : 0;

  return animate_swap(st, sw);
}


static Bool
done (struct state *st)
{
  int x, y;
  for (y = 0; y < st->height; y++)
    for (x = 0; x < st->width; x++)
      {
	int x2 = st->state[y * st->width + x].x;
	int y2 = st->state[y * st->width + x].y;
	if (x != x2 || y != y2)
	  return False;
      }
  return True;
}


static int
shuffle (struct state *st, Bool draw_p)
{
  struct piece *p1, *p2;
  int src_x, src_y, dst_x = -1, dst_y = -1;

 AGAIN:
  p1 = p2 = 0;
  src_x = random() % st->width;
  src_y = random() % st->height;

  get_piece(st, src_x, src_y, &p1, 0);

  /* Pick random coordinates until we find one that has the same kind of
     piece as the first one we picked.  Note that it's possible for there
     to be only one piece of a particular shape on the board (this always
     happens with the four corner pieces.)
   */
  while (p1 != p2)
    {
      dst_x = random() % st->width;
      dst_y = random() % st->height;
      get_piece(st, dst_x, dst_y, &p2, 0);
    }

  if (src_x == dst_x && src_y == dst_y)
    goto AGAIN;

  return swap_pieces(st, src_x, src_y, dst_x, dst_y, draw_p);
}


static void
shuffle_all (struct state *st)
{
  int j;
  for (j = 0; j < 5; j++) {
    /* swap each piece with another 5x */
    int i = (st->width * st->height * 5);
    while (--i > 0)
      shuffle (st, False);

    /* and do that whole process up to 5x if we ended up with a solved
       board (this often happens with 4x4 boards.) */
    if (!done(st)) 
      break;
  }
}


static int
unshuffle (struct state *st)
{
  int i;
  for (i = 0; i < st->width * st->height * 4; i++)
    {
      int x = random() % st->width;
      int y = random() % st->height;
      int x2 = st->state[y * st->width + x].x;
      int y2 = st->state[y * st->width + x].y;
      if (x != x2 || y != y2)
	{
	  return swap_pieces(st, x, y, x2, y2, True);
	}
    }
  return 0;
}


static int
animate_clear (struct state *st)
{
  while (st->clearing > 0)
    {
      int x = random() % st->width;
      int y = random() % st->height;
      XPoint *p = &st->state[y * st->width + x];
      if (p->x == -1)
	continue;
      draw_piece(st, p->x, p->y, 2);
      p->x = p->y = -1;
      st->clearing--;
      return st->delay;
    }
  return 0;
}


static int
clear_all (struct state *st)
{
  st->clearing = st->width * st->height;
  return animate_clear(st);
}


static void *
jigsaw_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  st->dpy = dpy;
  st->window = window;
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  st->delay2 = get_integer_resource (st->dpy, "delay2", "Integer") * 1000000;
  st->border_width = get_integer_resource (st->dpy, "pieceBorderWidth",
                                           "Integer");
  if (st->delay == 0) st->delay = 1; /* kludge */
  return st;
}


static unsigned long
jigsaw_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int x, y;
  int delay = 0;

  if (st->img_loader)   /* still loading */
    {
      st->img_loader = load_image_async_simple (st->img_loader, 0, 0, 0, 0, 0);
      if (! st->img_loader) {  /* just finished */
	shuffle_all (st);
	for (y = 0; y < st->height; y++)
	  for (x = 0; x < st->width; x++)
	    draw_piece(st, x, y, 0);
      }
      return st->delay;
    }

  if (st->swap.flashing)
    delay = animate_swap (st, &st->swap);
  else if (st->clearing)
    delay = animate_clear (st);

  if (!delay) {
    if (st->jigstate == 0)
      {
	jigsaw_init_1 (st);
	st->jigstate = 1;
      }
    else if (st->jigstate == 1)
      {
	if (done(st))
	  {
	    st->jigstate = 2;
	    delay = st->delay2;
	  }
	else
	  {
	    delay = unshuffle(st);
	  }
      }
    else if (st->jigstate == 2)
      {
	st->jigstate = 0;
	delay = clear_all(st);    
      }
    else
      abort();
  }

  if (delay == 1) delay = 0; /* kludge */
  return (delay ? delay : st->delay * 10);
}

static void
jigsaw_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  /* window size is checked each time a new puzzle begins */
}

static Bool
jigsaw_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
jigsaw_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free_puzzle_pixmaps (st);
  if (st->state) free (st->state);
  if (st->gc) XFreeGC (dpy, st->gc);
  if (st->source) XFreePixmap (dpy, st->source);
  free (st);
}



static const char *jigsaw_defaults [] = {
  ".background:		Black",
  ".foreground:		#AAAAAA",
  "*fpsSolid:		true",
  "*delay:		70000",
  "*delay2:		5",
  "*pieceBorderWidth:   -1",
#ifdef __sgi    /* really, HAVE_READ_DISPLAY_EXTENSION */
  "*visualID:		Best",
#endif
  0
};

static XrmOptionDescRec jigsaw_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",		XrmoptionSepArg, 0 },
  { "-bw",		".pieceBorderWidth",	XrmoptionSepArg, 0 },
  { "-border-width",	".pieceBorderWidth",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Jigsaw", jigsaw)
