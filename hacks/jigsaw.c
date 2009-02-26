/* xscreensaver, Copyright (c) 1997, 1998 Jamie Zawinski <jwz@jwz.org>
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

    =  The shapes of the piece bitmaps still aren't quite right.
       They should line up with no overlap.  They don't...
    
    =  Have it drop all pieces to the "floor" then pick them up to
       reassemble the picture.

    =  As a joke, maybe sometimes have one piece that doesn't fit?
       Or lose a piece?
 */

#include "screenhack.h"

#define DEBUG

#include "images/jigsaw/jigsaw_a_h.xbm"
#include "images/jigsaw/jigsaw_a_n_h.xbm"
#include "images/jigsaw/jigsaw_a_ne_h.xbm"
#include "images/jigsaw/jigsaw_a_e_h.xbm"
#include "images/jigsaw/jigsaw_a_se_h.xbm"
#include "images/jigsaw/jigsaw_a_s_h.xbm"
#include "images/jigsaw/jigsaw_a_sw_h.xbm"
#include "images/jigsaw/jigsaw_a_w_h.xbm"
#include "images/jigsaw/jigsaw_a_nw_h.xbm"

#include "images/jigsaw/jigsaw_b_h.xbm"
#include "images/jigsaw/jigsaw_b_n_h.xbm"
#include "images/jigsaw/jigsaw_b_ne_h.xbm"
#include "images/jigsaw/jigsaw_b_e_h.xbm"
#include "images/jigsaw/jigsaw_b_se_h.xbm"
#include "images/jigsaw/jigsaw_b_s_h.xbm"
#include "images/jigsaw/jigsaw_b_sw_h.xbm"
#include "images/jigsaw/jigsaw_b_w_h.xbm"
#include "images/jigsaw/jigsaw_b_nw_h.xbm"

#include "images/jigsaw/jigsaw_a_f.xbm"
#include "images/jigsaw/jigsaw_a_n_f.xbm"
#include "images/jigsaw/jigsaw_a_ne_f.xbm"
#include "images/jigsaw/jigsaw_a_e_f.xbm"
#include "images/jigsaw/jigsaw_a_se_f.xbm"
#include "images/jigsaw/jigsaw_a_s_f.xbm"
#include "images/jigsaw/jigsaw_a_sw_f.xbm"
#include "images/jigsaw/jigsaw_a_w_f.xbm"
#include "images/jigsaw/jigsaw_a_nw_f.xbm"

#include "images/jigsaw/jigsaw_b_f.xbm"
#include "images/jigsaw/jigsaw_b_n_f.xbm"
#include "images/jigsaw/jigsaw_b_ne_f.xbm"
#include "images/jigsaw/jigsaw_b_e_f.xbm"
#include "images/jigsaw/jigsaw_b_se_f.xbm"
#include "images/jigsaw/jigsaw_b_s_f.xbm"
#include "images/jigsaw/jigsaw_b_sw_f.xbm"
#include "images/jigsaw/jigsaw_b_w_f.xbm"
#include "images/jigsaw/jigsaw_b_nw_f.xbm"

#define GRID_WIDTH  66
#define GRID_HEIGHT 66

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

static struct set all_pieces[4];

static void
init_images(Display *dpy, Window window)
{
# define LOAD_PIECE(PIECE,NAME)					\
    PIECE.x = jigsaw_##NAME##_x_hot;				\
    PIECE.y = jigsaw_##NAME##_y_hot;				\
    PIECE.pixmap =						\
    XCreatePixmapFromBitmapData(dpy, window,			\
				(char *) jigsaw_##NAME##_bits,	\
				jigsaw_##NAME##_width,		\
				jigsaw_##NAME##_height,		\
				1, 0, 1)

# define LOAD_PIECES(SET,PREFIX,SUFFIX)				\
    LOAD_PIECE(SET.pieces[CENTER],    PREFIX##_##SUFFIX);	\
    LOAD_PIECE(SET.pieces[NORTH],     PREFIX##_n_##SUFFIX);	\
    LOAD_PIECE(SET.pieces[NORTHEAST], PREFIX##_ne_##SUFFIX);	\
    LOAD_PIECE(SET.pieces[EAST],      PREFIX##_e_##SUFFIX);	\
    LOAD_PIECE(SET.pieces[SOUTHEAST], PREFIX##_se_##SUFFIX);	\
    LOAD_PIECE(SET.pieces[SOUTH],     PREFIX##_s_##SUFFIX);	\
    LOAD_PIECE(SET.pieces[SOUTHWEST], PREFIX##_sw_##SUFFIX);	\
    LOAD_PIECE(SET.pieces[WEST],      PREFIX##_w_##SUFFIX);	\
    LOAD_PIECE(SET.pieces[NORTHWEST], PREFIX##_nw_##SUFFIX)

  LOAD_PIECES(all_pieces[PIECE_A_HOLLOW],a,h);
  LOAD_PIECES(all_pieces[PIECE_A_FILLED],a,f);
  LOAD_PIECES(all_pieces[PIECE_B_HOLLOW],b,h);
  LOAD_PIECES(all_pieces[PIECE_B_FILLED],b,f);

# undef LOAD_PIECE
# undef LOAD_PIECES
}

static Pixmap
read_screen (Display *dpy, Window window, int *widthP, int *heightP)
{
  Pixmap p;
  XWindowAttributes xgwa;
  XGCValues gcv;
  GC gc;
  XGetWindowAttributes (dpy, window, &xgwa);
  *widthP = xgwa.width;
  *heightP = xgwa.height;

  XClearWindow(dpy, window);
  grab_screen_image(xgwa.screen, window);
  p = XCreatePixmap(dpy, window, *widthP, *heightP, xgwa.depth);
  gcv.function = GXcopy;
  gc = XCreateGC (dpy, window, GCFunction, &gcv);
  XCopyArea (dpy, window, p, gc, 0, 0, *widthP, *heightP, 0, 0);

  XFreeGC (dpy, gc);

  return p;
}


static int width, height;
static int x_border, y_border;
static Pixmap source;
static GC gc;
static Bool tweak;
static int fg, bg;
static XPoint *state = 0;

static void
jigsaw_init(Display *dpy, Window window)
{
  XWindowAttributes xgwa;
  int x, y;
  XGCValues gcv;
  Colormap cmap;
  int source_w, source_h;

  tweak = random()&1;

  source = read_screen (dpy, window, &source_w, &source_h);

  XGetWindowAttributes (dpy, window, &xgwa);
  cmap = xgwa.colormap;
  width  = xgwa.width  / GRID_WIDTH;
  height = xgwa.height / GRID_HEIGHT;
  x_border = (xgwa.width  - (width  * GRID_WIDTH)) / 2;
  y_border = (xgwa.height - (height * GRID_WIDTH)) / 2;

  if (!state)
    state = (XPoint *) malloc(width * height * sizeof(XPoint));
  gc = XCreateGC (dpy, window, 0, &gcv);

  {
    XColor fgc, bgc;
    char *fgs = get_string_resource("foreground", "Foreground");
    char *bgs = get_string_resource("background", "Background");
    Bool fg_ok, bg_ok;
    if (!XParseColor (dpy, cmap, fgs, &fgc))
      XParseColor (dpy, cmap, "gray", &fgc);
    if (!XParseColor (dpy, cmap, bgs, &bgc))
      XParseColor (dpy, cmap, "black", &bgc);

    fg_ok = XAllocColor (dpy, cmap, &fgc);
    bg_ok = XAllocColor (dpy, cmap, &bgc);

    /* If we weren't able to allocate the two colors we want from the
       colormap (which is likely if the screen has been grabbed on an
       8-bit SGI visual -- don't ask) then just go through the map
       and find the closest color to the ones we wanted, and use those
       pixels without actually allocating them.
     */
    if (fg_ok)
      fg = fgc.pixel;
    else
      fg = 0;

    if (bg_ok)
      bg = bgc.pixel;
    else
      bg = 1;

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
	XQueryColors (dpy, cmap, all, max);
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
		    fg = all[i].pixel;
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
		    bg = all[i].pixel;
		    if (d == 0)
		      bg_ok = True;
		  }
	      }

	    if (fg_ok && bg_ok)
	      break;
	  }
	XFree(all);
      }
  }

  /* Reset the window's background color... */
  XSetWindowBackground (dpy, window, bg);
  XClearWindow(dpy, window);

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
	state[y * width + x].x = x;
	state[y * width + x].y = y;
      }
}


static void
get_piece(int x, int y, struct piece **hollow, struct piece **filled)
{
  int p;
  Bool which = (x & 1) == (y & 1);

  if      (x == 0       && y == 0)	  p = NORTHWEST;
  else if (x == width-1 && y == 0)	  p = NORTHEAST;
  else if (x == width-1 && y == height-1) p = SOUTHEAST;
  else if (x == 0       && y == height-1) p = SOUTHWEST;
  else if (y == 0)			  p = NORTH;
  else if (x == width-1)		  p = EAST;
  else if (y == height-1)		  p = SOUTH;
  else if (x == 0)			  p = WEST;
  else					  p = CENTER;

  if (tweak) which = !which;
  if (hollow)
    *hollow = (which
	       ? &all_pieces[PIECE_A_HOLLOW].pieces[p]
	       : &all_pieces[PIECE_B_HOLLOW].pieces[p]);
  if (filled)
    *filled = (which
	       ? &all_pieces[PIECE_A_FILLED].pieces[p]
	       : &all_pieces[PIECE_B_FILLED].pieces[p]);
}


static void
draw_piece(Display *dpy, Window window, int x, int y, int clear_p)
{
  struct piece *hollow, *filled;
  int from_x = state[y * width + x].x;
  int from_y = state[y * width + x].y;

  get_piece(x, y, &hollow, &filled);
	  
  XSetClipMask(dpy, gc, filled->pixmap);
  XSetClipOrigin(dpy, gc,
		 x_border + (x * GRID_WIDTH) - filled->x - 1,
		 y_border + (y * GRID_WIDTH) - filled->y - 1);

  if (clear_p)
    {
      XSetForeground(dpy, gc, bg);
      XFillRectangle(dpy, window, gc,
		     x_border + (x * GRID_WIDTH)  - GRID_WIDTH/2,
		     y_border + (y * GRID_HEIGHT) - GRID_HEIGHT/2,
		     GRID_WIDTH*2, GRID_HEIGHT*2);
    }
  else
    XCopyArea(dpy, source, window, gc,
	      x_border + (from_x * GRID_WIDTH)  - GRID_WIDTH/2,
	      y_border + (from_y * GRID_HEIGHT) - GRID_HEIGHT/2,
	      GRID_WIDTH*2, GRID_HEIGHT*2,
	      x_border + (x * GRID_WIDTH)  - GRID_WIDTH/2,
	      y_border + (y * GRID_HEIGHT) - GRID_HEIGHT/2);

  if (clear_p > 1)
    return;

  XSetForeground(dpy, gc, fg);
  XSetClipMask(dpy, gc, hollow->pixmap);
  XSetClipOrigin(dpy, gc,
		 x_border + (x * GRID_WIDTH) - hollow->x - 1,
		 y_border + (y * GRID_WIDTH) - hollow->y - 1);
  XFillRectangle(dpy, window, gc,
		 x_border + (x * GRID_WIDTH)  - GRID_WIDTH/2,
		 y_border + (y * GRID_HEIGHT) - GRID_HEIGHT/2,
		 GRID_WIDTH*2, GRID_HEIGHT*2);

  if (clear_p)
    {
      /* If the pieces lined up right, we could do this by just not drawing
	 the outline -- but that doesn't look right, since it eats the outlines
	 of the adjascent pieces.  So draw the outline, then chop off the outer
	 edge if this is a border piece.
       */
      XSetForeground(dpy, gc, bg);
      if (x == 0)
	XFillRectangle(dpy, window, gc,
		       x_border - 2,
		       y_border + (y * GRID_HEIGHT),
		       3, GRID_HEIGHT);
      else if (x == width-1)
	XFillRectangle(dpy, window, gc,
		       x_border + ((x+1) * GRID_WIDTH) - 2,
		       y_border + (y * GRID_HEIGHT),
		       3, GRID_HEIGHT);

      if (y == 0)
	XFillRectangle(dpy, window, gc,
		       x_border + (x * GRID_WIDTH),
		       y_border - 2,
		       GRID_WIDTH, 3);
      else if (y == height-1)
	XFillRectangle(dpy, window, gc,
		       x_border + (x * GRID_WIDTH),
		       y_border + ((y+1) * GRID_HEIGHT) - 2,
		       GRID_WIDTH, 3);
    }
}


static void
swap_pieces(Display *dpy, Window window,
	    int src_x, int src_y, int dst_x, int dst_y,
	    Bool draw_p)
{
  XPoint swap;
  int i;
  if (draw_p)
    for (i = 0; i < 3; i++)
      {
	draw_piece(dpy, window, src_x, src_y, 1);
	draw_piece(dpy, window, dst_x, dst_y, 1);
	XSync(dpy, False);
	usleep(50000);
	draw_piece(dpy, window, src_x, src_y, 0);
	draw_piece(dpy, window, dst_x, dst_y, 0);
	XSync(dpy, False);
	usleep(50000);
      }

  swap = state[src_y * width + src_x];
  state[src_y * width + src_x] = state[dst_y * width + dst_x];
  state[dst_y * width + dst_x] = swap;

  if (draw_p)
    {
      draw_piece(dpy, window, src_x, src_y, 0);
      draw_piece(dpy, window, dst_x, dst_y, 0);
      XSync(dpy, False);
    }
}


static void
shuffle(Display *dpy, Window window, Bool draw_p)
{
  struct piece *p1, *p2;
  int src_x, src_y, dst_x = -1, dst_y = -1;

 AGAIN:
  p1 = p2 = 0;
  src_x = random() % width;
  src_y = random() % height;

  get_piece(src_x, src_y, &p1, 0);

  /* Pick random coordinates until we find one that has the same kind of
     piece as the first one we picked.  Note that it's possible for there
     to be only one piece of a particular shape on the board (this commonly
     happens with the corner pieces.)
   */
  while (p1 != p2)
    {
      dst_x = random() % width;
      dst_y = random() % height;
      get_piece(dst_x, dst_y, &p2, 0);
    }

  if (src_x == dst_x && src_y == dst_y)
    goto AGAIN;

  swap_pieces(dpy, window, src_x, src_y, dst_x, dst_y, draw_p);
}


static void
shuffle_all(Display *dpy, Window window)
{
  int i = (width * height * 10);
  while (i > 0)
    {
      shuffle(dpy, window, False);
      i--;
    }
}

static void
unshuffle(Display *dpy, Window window)
{
  int i;
  for (i = 0; i < width * height * 4; i++)
    {
      int x = random() % width;
      int y = random() % height;
      int x2 = state[y * width + x].x;
      int y2 = state[y * width + x].y;
      if (x != x2 || y != y2)
	{
	  swap_pieces(dpy, window, x, y, x2, y2, True);
	  break;
	}
    }
}

static void
clear_all(Display *dpy, Window window)
{
  int n = width * height;
  while (n > 0)
    {
      int x = random() % width;
      int y = random() % height;
      XPoint *p = &state[y * width + x];
      if (p->x == -1)
	continue;
      draw_piece(dpy, window, p->x, p->y, 2);
      XSync(dpy, False);
      usleep(1000);
      p->x = p->y = -1;
      n--;
    }
}

static Bool
done(void)
{
  int x, y;
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
	int x2 = state[y * width + x].x;
	int y2 = state[y * width + x].y;
	if (x != x2 || y != y2)
	  return False;
      }
  return True;
}



char *progclass = "Jigsaw";

char *defaults [] = {
  ".background:		Black",
  ".foreground:		Gray40",
  "*delay:		70000",
  "*delay2:		5",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-delay2",		".delay2",		XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

void
screenhack (Display *dpy, Window window)
{
  int delay = get_integer_resource("delay", "Integer");
  int delay2 = get_integer_resource("delay2", "Integer");

  init_images(dpy, window);

  while (1)
    {
      int x, y;
      jigsaw_init (dpy, window);
      shuffle_all(dpy, window);

      for (y = 0; y < height; y++)
	for (x = 0; x < width; x++)
	  draw_piece(dpy, window, x, y, 0);

      while (!done())
	{
	  unshuffle(dpy, window);
	  XSync (dpy, True);
	  if (delay) usleep (delay);
	}

      if (delay2)
	usleep (delay2 * 1000000);

      clear_all(dpy, window);
    }
}
