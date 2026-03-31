/* forest.c (aka xtree.c), Copyright (c) 1999
 *  Peter Baumung <unn6@rz.uni-karlsruhe.de>
 *
 * Most code taken from
 *  xscreensaver, Copyright (c) 1992, 1995, 1997 
 *  Jamie Zawinski <jwz@netscape.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* ******************************   NOTE   ******************************

     This is not the xlockmore version of forest, but a much better
     looking rewrite.  Be careful not to delete it in a merging frenzy...

   **********************************************************************
 */

#ifdef STANDALONE
# define DEFAULTS   "*delay:            500000 \n"  \
                    "*ncolors:          20     \n" \
                    "*fpsSolid:          true     \n" \

# include "xlockmore.h"     /* from the xscreensaver distribution */
# define free_trees 0
# define release_trees 0
# define reshape_trees 0
# define trees_handle_event 0
#else  /* !STANDALONE */
# include "xlock.h"					/* from the xlockmore distribution */
#endif /* !STANDALONE */

ENTRYPOINT ModeSpecOpt trees_opts = {0, NULL, 0, NULL, NULL};

typedef struct {
  int     x;
  int     y;
  int     thick;
  double  size;
  long    color;
  int     toDo;
  int     pause;
  int     season;
} treestruct;

static treestruct *trees = NULL;

static XColor colors[20];
static int color;

static long colorM[12] = {0xff0000, 0xff8000, 0xffff00, 0x80ff00,
                          0x00ff00, 0x00ff80, 0x00ffff, 0x0080ff,
                          0x0000ff, 0x8000ff, 0xff00ff, 0xff0080};

static long colorV[12] = {0x0a0000, 0x0a0500, 0x0a0a00, 0x050a00,
                          0x000a00, 0x000a05, 0x000a0a, 0x00050a,
                          0x00000a, 0x05000a, 0x0a000a, 0x0a0005};

ENTRYPOINT void
init_trees(ModeInfo * mi)
{
  treestruct       *tree;
  Display          *display = MI_DISPLAY(mi);
  GC                gc = MI_GC(mi);
  int               i;

  if (trees == NULL) {
		
    if (mi->npixels > 20) {
			printf("%d colors selected. Setting limit to 20...\n", mi->npixels);
			mi->npixels = 20;
		}
			
		if (mi->npixels < 4) {
      for (i = 0; i < mi->npixels; i++) {
        colors[i].red   = 65535 * (i & 1);
        colors[i].green = 65535 * (i & 1);
        colors[i].blue  = 65535 * (i & 1);
        colors[i].flags = DoRed | DoGreen | DoBlue;
      }
    } else {
      if (mi->npixels < 8) {
        for (i = 0; i < mi->npixels; i++) {
          colors[i].red   = 32768 + 4096 * (i % 4);
          colors[i].green = 32768 + 4096 * (i % 4);
          colors[i].blue  = 32768 + 4096 * (i % 4);
          colors[i].flags = DoRed | DoGreen | DoBlue;
        }
      } else {
        for (i = 0; i < mi->npixels; i++) {
          colors[i].red   = 24576 + 4096 * (i % 4);
          colors[i].green = 10240 + 2048 * (i % 4);
          colors[i].blue  =  0;
          colors[i].flags = DoRed | DoGreen | DoBlue;
        }
      }
    }

    for (i = 0; i < mi->npixels; i++)
      if (!XAllocColor(display, mi->xgwa.colormap, &colors[i])) break;
    color = i;

    XSetForeground(display, gc, colors[1].pixel);
  }

  MI_INIT (mi, trees);

  XClearWindow(display, MI_WINDOW(mi));
  XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinMiter);
  tree = &trees[MI_SCREEN(mi)];
  tree->toDo   = 25;
	tree->season = NRAND(12);

  for (i = 4; i < mi->npixels; i++) {
    int sIndex = (tree->season + (i-4) / 4) % 12;
    long color = colorM[sIndex] - 2 * colorV[sIndex] * (i % 4);
    colors[i].red = (color & 0xff0000) / 256;
    colors[i].green = (color & 0x00ff00);
    colors[i].blue = (color & 0x0000ff) * 256;
    colors[i].flags = DoRed | DoGreen | DoBlue;
  }

  for (i = 0; i < mi->npixels; i++)
    if (!XAllocColor(display, mi->xgwa.colormap, &colors[i])) break;

  color = i;
}

static double rRand(double a, double b)
{
  return (a+(b-a)*NRAND(10001)/10000.0);
}

static void draw_line(ModeInfo * mi,
                      int x1, int y1, int x2, int y2,
                      double angle, int widths, int widthe)
{

  Display    *display = MI_DISPLAY(mi);
  GC          gc = MI_GC(mi);
  double      sns = 0.5*widths*sin(angle + M_PI_2);
  double      css = 0.5*widths*cos(angle + M_PI_2);
  double      sne = 0.5*widthe*sin(angle + M_PI_2);
  double      cse = 0.5*widthe*cos(angle + M_PI_2);

  int         xs1 = (int) (x1-sns);
  int         xs2 = (int) (x1+sns);
  int         ys1 = (int) (y1-css);
  int         ys2 = (int) (y1+css);
  int         xe1 = (int) (x2-sne);
  int         xe2 = (int) (x2+sne);
  int         ye1 = (int) (y2-cse);
  int         ye2 = (int) (y2+cse);
  int         i;

  for (i = 0; i < widths; i++) {
    if (color >= 4)
      XSetForeground(display, gc, colors[i*4/widths].pixel);
    XDrawLine(display, MI_WINDOW(mi), gc,
      xs1+(xs2-xs1)*i/widths, ys1+(ys2-ys1)*i/widths,
      xe1+(xe2-xe1)*i/widths, ye1+(ye2-ye1)*i/widths);
  }
}

static void draw_tree_rec(ModeInfo * mi, double thick, int x, int y, double angle)
{
  treestruct *tree = &trees[MI_SCREEN(mi)];
  Display    *display = MI_DISPLAY(mi);
  GC          gc = MI_GC(mi);
  int         length = (24+NRAND(12))*tree->size;
  int         a = (int) (x - length*sin(angle));
  int         b = (int) (y - length*cos(angle));
  int         i;

  draw_line(mi, x, y, a, b, angle, thick*tree->size, 0.68*thick*tree->size);

  if (thick > 2) {
    draw_tree_rec(mi, 0.68*thick, a, b, 0.8*angle+rRand(-0.2, 0.2));
    if (thick < tree->thick-1) {
      draw_tree_rec(mi, 0.68*thick, a, b, angle+rRand(0.2, 0.9));
      draw_tree_rec(mi, 0.68*thick, (a+x)/2, (b+y)/2, angle-rRand(0.2, 0.9));
    }
  }

  if (thick < 0.5*tree->thick) {
  	int     nleaf = 12 + NRAND(4);
	  XArc    leaf[16];
    for (i = 0; i < nleaf; i++) {
      leaf[i].x = a + (int) (tree->size * rRand(-12, 12));
      leaf[i].y = b + (int) (tree->size * rRand(-12, 12));
      leaf[i].width = (int) (tree->size * rRand(2, 6));
      leaf[i].height = leaf[i].width;
      leaf[i].angle1 = 0;
      leaf[i].angle2 = 360 * 64;
    }
    if (mi->npixels >= 4)
      XSetForeground(display, gc, colors[tree->color+NRAND(4)].pixel);
    XFillArcs(display, MI_WINDOW(mi), gc, leaf, nleaf);
  }
}

ENTRYPOINT void
draw_trees(ModeInfo * mi)
{
  treestruct *tree = &trees[MI_SCREEN(mi)];
	int					width = MI_WIN_WIDTH(mi);
	int					height = MI_WIN_HEIGHT(mi);

  if (tree->pause == 1) {
    tree->pause--;
    init_trees(mi);
  } else if (tree->pause > 1) {
    tree->pause--;
    return;
  } else if (--(tree->toDo) == 0) {
    tree->pause = 6;
    return;
  }

  tree->x        = NRAND(width);
  tree->y        = (int) (1.25 * height * (1 - tree->toDo / 23.0));
  tree->thick    = rRand(7, 12);
  tree->size     = height / 480.0;
  if (color < 8) {
    tree->color = 0;
  } else {
    tree->color    = 4 * (1 + NRAND(color / 4 - 1));
  }

  draw_tree_rec(mi, tree->thick, tree->x, tree->y, rRand(-0.1, 0.1));
}


XSCREENSAVER_MODULE_2 ("Forest", forest, trees)
