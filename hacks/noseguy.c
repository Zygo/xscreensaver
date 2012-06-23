/* xscreensaver, Copyright (c) 1992-2012 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* Make a little guy with a big nose and a hat wanter around the screen,
   spewing out messages.  Derived from xnlock by 
   Dan Heller <argv@danheller.com>.
 */

#include "screenhack.h"
#include "xpm-pixmap.h"
#include "textclient.h"

#ifdef HAVE_COCOA
# define HAVE_XPM
#endif

#define font_height(font) (font->ascent + font->descent)


struct state {
  Display *dpy;
  Window window;
  int Width, Height;
  GC fg_gc, bg_gc, text_fg_gc, text_bg_gc;
  int x, y;
  XFontStruct *font;

  unsigned long interval;
  Pixmap left1, left2, right1, right2;
  Pixmap left_front, right_front, front, down;

  char *program;
  text_data *tc;

  int state;	/* indicates states: walking or getting passwd */
  int first_time;

  void (*next_fn) (struct state *);

  int move_length, move_dir;

  int      walk_lastdir;
  int      walk_up;
  Pixmap   walk_frame;

  int X, Y, talking;

  struct {
    int x, y, width, height;
  } s_rect;

  char words[10240];
  int lines;
};

static void fill_words (struct state *);
static void walk (struct state *, int dir);
static void talk (struct state *, int erase);
static void talk_1 (struct state *);
static int think (struct state *);
static unsigned long look (struct state *); 

#define IS_MOVING  1

#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
# include "images/noseguy/nose-f1.xpm"
# include "images/noseguy/nose-f2.xpm"
# include "images/noseguy/nose-f3.xpm"
# include "images/noseguy/nose-f4.xpm"
# include "images/noseguy/nose-l1.xpm"
# include "images/noseguy/nose-l2.xpm"
# include "images/noseguy/nose-r1.xpm"
# include "images/noseguy/nose-r2.xpm"
#else
# include "images/noseguy/nose-f1.xbm"
# include "images/noseguy/nose-f2.xbm"
# include "images/noseguy/nose-f3.xbm"
# include "images/noseguy/nose-f4.xbm"
# include "images/noseguy/nose-l1.xbm"
# include "images/noseguy/nose-l2.xbm"
# include "images/noseguy/nose-r1.xbm"
# include "images/noseguy/nose-r2.xbm"
#endif

static void
init_images (struct state *st)
{
  Pixmap *images[8];
#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)
  char **bits[8];
#else
  unsigned char *bits[8];
#endif

  int i = 0;
  images[i++] = &st->left1;
  images[i++] = &st->left2;
  images[i++] = &st->right1;
  images[i++] = &st->right2;
  images[i++] = &st->left_front;
  images[i++] = &st->right_front;
  images[i++] = &st->front;
  images[i]   = &st->down;

#if defined(HAVE_GDK_PIXBUF) || defined(HAVE_XPM)

  i = 0;
  bits[i++] = nose_l1_xpm;
  bits[i++] = nose_l2_xpm;
  bits[i++] = nose_r1_xpm;
  bits[i++] = nose_r2_xpm;
  bits[i++] = nose_f2_xpm;
  bits[i++] = nose_f3_xpm;
  bits[i++] = nose_f1_xpm;
  bits[i]   = nose_f4_xpm;

  for (i = 0; i < sizeof (images) / sizeof(*images); i++)
    {
      Pixmap pixmap = xpm_data_to_pixmap (st->dpy, st->window, bits[i],
                                          0, 0, 0);
      if (!pixmap)
	{
	  fprintf (stderr, "%s: Can't load nose images\n", progname);
	  exit (1);
	}
      *images[i] = pixmap;
    }
#else
  i = 0;
  bits[i++] = nose_l1_bits;
  bits[i++] = nose_l2_bits;
  bits[i++] = nose_r1_bits;
  bits[i++] = nose_r2_bits;
  bits[i++] = nose_f2_bits;
  bits[i++] = nose_f3_bits;
  bits[i++] = nose_f1_bits;
  bits[i++] = nose_f4_bits;

  for (i = 0; i < sizeof (images) / sizeof(*images); i++)
    if (!(*images[i] =
	  XCreatePixmapFromBitmapData(st->dpy, st->window,
				      (char *) bits[i], 64, 64, 1, 0, 1)))
      {
	fprintf (stderr, "%s: Can't load nose images\n", progname);
	exit (1);
      }
#endif
}

#define LEFT 	001
#define RIGHT 	002
#define DOWN 	004
#define UP 	010
#define FRONT	020
#define X_INCR 3
#define Y_INCR 2

static void
move (struct state *st)
{
    if (!st->move_length)
    {
	register int    tries = 0;
	st->move_dir = 0;
	if ((random() & 1) && think(st))
	{
	    talk(st, 0);		/* sets timeout to itself */
	    return;
	}
	if (!(random() % 3) && (st->interval = look(st)))
	{
	    st->next_fn = move;
	    return;
	}
	st->interval = 20 + random() % 100;
	do
	{
	    if (!tries)
		st->move_length = st->Width / 100 + random() % 90, tries = 8;
	    else
		tries--;
	    /* There maybe the case that we won't be able to exit from
	       this routine (especially when the geometry is too small)!!

	       Ensure that we can exit from this routine.
	     */
#if 1
	    if (!tries && (st->move_length <= 1)) {
	      st->move_length = 1;
	      break;
	    }
#endif
	    switch (random() % 8)
	    {
	    case 0:
		if (st->x - X_INCR * st->move_length >= 5)
		    st->move_dir = LEFT;
		break;
	    case 1:
		if (st->x + X_INCR * st->move_length <= st->Width - 70)
		    st->move_dir = RIGHT;
		break;
	    case 2:
		if (st->y - (Y_INCR * st->move_length) >= 5)
		    st->move_dir = UP, st->interval = 40;
		break;
	    case 3:
		if (st->y + Y_INCR * st->move_length <= st->Height - 70)
		    st->move_dir = DOWN, st->interval = 20;
		break;
	    case 4:
		if (st->x - X_INCR * st->move_length >= 5 && st->y - (Y_INCR * st->move_length) >= 5)
		    st->move_dir = (LEFT | UP);
		break;
	    case 5:
		if (st->x + X_INCR * st->move_length <= st->Width - 70 &&
		    st->y - Y_INCR * st->move_length >= 5)
		    st->move_dir = (RIGHT | UP);
		break;
	    case 6:
		if (st->x - X_INCR * st->move_length >= 5 &&
		    st->y + Y_INCR * st->move_length <= st->Height - 70)
		    st->move_dir = (LEFT | DOWN);
		break;
	    case 7:
		if (st->x + X_INCR * st->move_length <= st->Width - 70 &&
		    st->y + Y_INCR * st->move_length <= st->Height - 70)
		    st->move_dir = (RIGHT | DOWN);
		break;
	    default:
		/* No Defaults */
		break;
	    }
	} while (!st->move_dir);
    }
    if (st->move_dir)
      walk(st, st->move_dir);
    --st->move_length;
    st->next_fn = move;
}

#ifdef HAVE_XPM
# define COPY(dpy,frame,window,gc,x,y,w,h,x2,y2) \
  XCopyArea (dpy,frame,window,gc,x,y,w,h,x2,y2)
#else
# define COPY(dpy,frame,window,gc,x,y,w,h,x2,y2) \
  XCopyPlane(dpy,frame,window,gc,x,y,w,h,x2,y2,1L)
#endif

static void
walk (struct state *st, int dir)
{
    register int    incr = 0;

    if (dir & (LEFT | RIGHT))
    {				/* left/right movement (mabye up/st->down too) */
	st->walk_up = -st->walk_up;		/* bouncing effect (even if hit a wall) */
	if (dir & LEFT)
	{
	    incr = X_INCR;
	    st->walk_frame = (st->walk_up < 0) ? st->left1 : st->left2;
	}
	else
	{
	    incr = -X_INCR;
	    st->walk_frame = (st->walk_up < 0) ? st->right1 : st->right2;
	}
	if ((st->walk_lastdir == FRONT || st->walk_lastdir == DOWN) && dir & UP)
	{

	    /*
	     * workaround silly bug that leaves screen dust when guy is
	     * facing forward or st->down and moves up-left/right.
	     */
	    COPY(st->dpy, st->walk_frame, st->window, st->fg_gc, 0, 0, 64, 64, st->x, st->y);
	}
	/* note that maybe neither UP nor DOWN is set! */
	if (dir & UP && st->y > Y_INCR)
	    st->y -= Y_INCR;
	else if (dir & DOWN && st->y < st->Height - 64)
	    st->y += Y_INCR;
    }
    /* Explicit up/st->down movement only (no left/right) */
    else if (dir == UP)
	COPY(st->dpy, st->front, st->window, st->fg_gc, 0, 0, 64, 64, st->x, st->y -= Y_INCR);
    else if (dir == DOWN)
	COPY(st->dpy, st->down, st->window, st->fg_gc, 0, 0, 64, 64, st->x, st->y += Y_INCR);
    else if (dir == FRONT && st->walk_frame != st->front)
    {
	if (st->walk_up > 0)
	    st->walk_up = -st->walk_up;
	if (st->walk_lastdir & LEFT)
	    st->walk_frame = st->left_front;
	else if (st->walk_lastdir & RIGHT)
	    st->walk_frame = st->right_front;
	else
	    st->walk_frame = st->front;
	COPY(st->dpy, st->walk_frame, st->window, st->fg_gc, 0, 0, 64, 64, st->x, st->y);
    }
    if (dir & LEFT)
	while (--incr >= 0)
	{
	    COPY(st->dpy, st->walk_frame, st->window, st->fg_gc, 0, 0, 64, 64, --st->x, st->y + st->walk_up);
	}
    else if (dir & RIGHT)
	while (++incr <= 0)
	{
	    COPY(st->dpy, st->walk_frame, st->window, st->fg_gc, 0, 0, 64, 64, ++st->x, st->y + st->walk_up);
	}
    st->walk_lastdir = dir;
}

static int
think (struct state *st)
{
    if (random() & 1)
	walk(st, FRONT);
    if (random() & 1)
      return 1;
    return 0;
}

#define MAXLINES 10

static void
talk (struct state *st, int force_erase)
{
    int             width = 0,
                    height,
                    Z,
                    total = 0;
    register char  *p,
                   *p2;
    char            args[MAXLINES][256];

    /* clear what we've written */
    if (st->talking || force_erase)
    {
	if (!st->talking)
	    return;
	XFillRectangle(st->dpy, st->window, st->bg_gc, st->s_rect.x - 5, st->s_rect.y - 5,
		       st->s_rect.width + 10, st->s_rect.height + 10);
	st->talking = 0;
	if (!force_erase)
	  st->next_fn = move;
	st->interval = 0;
	{
	  /* might as well check the st->window for size changes now... */
	  XWindowAttributes xgwa;
	  XGetWindowAttributes (st->dpy, st->window, &xgwa);
	  st->Width = xgwa.width + 2;
	  st->Height = xgwa.height + 2;
	}
	return;
    }
    st->talking = 1;
    walk(st, FRONT);
    p = st->words;

    for (p2 = p; *p2; p2++)
      if (*p2 == '\t') *p2 = ' ';

    if (!(p2 = strchr(p, '\n')) || !p2[1])
      {
	total = strlen (st->words);
	strcpy (args[0], st->words);
	width = XTextWidth(st->font, st->words, total);
	height = 0;
      }
    else
      /* p2 now points to the first '\n' */
      for (height = 0; p; height++)
	{
	  int             w;
	  *p2 = 0;
	  if ((w = XTextWidth(st->font, p, p2 - p)) > width)
	    width = w;
	  total += p2 - p;	/* total chars; count to determine reading
				 * time */
	  (void) strcpy(args[height], p);
	  if (height == MAXLINES - 1)
	    {
	      /* puts("Message too long!"); */
	      break;
	    }
	  p = p2 + 1;
	  if (!(p2 = strchr(p, '\n')))
	    break;
	}
    height++;

    /*
     * Figure out the height and width in pixels (height, width) extend the
     * new box by 15 pixels on the sides (30 total) top and bottom.
     */
    st->s_rect.width = width + 30;
    st->s_rect.height = height * font_height(st->font) + 30;
    if (st->x - st->s_rect.width - 10 < 5)
	st->s_rect.x = 5;
    else if ((st->s_rect.x = st->x + 32 - (st->s_rect.width + 15) / 2)
	     + st->s_rect.width + 15 > st->Width - 5)
	st->s_rect.x = st->Width - 15 - st->s_rect.width;
    if (st->y - st->s_rect.height - 10 < 5)
	st->s_rect.y = st->y + 64 + 5;
    else
	st->s_rect.y = st->y - 5 - st->s_rect.height;

    XFillRectangle(st->dpy, st->window, st->text_bg_gc,
	 st->s_rect.x, st->s_rect.y, st->s_rect.width, st->s_rect.height);

    /* make a box that's 5 pixels thick. Then add a thin box inside it */
    XSetLineAttributes(st->dpy, st->text_fg_gc, 5, 0, 0, 0);
    XDrawRectangle(st->dpy, st->window, st->text_fg_gc,
		   st->s_rect.x, st->s_rect.y, st->s_rect.width - 1, st->s_rect.height - 1);
    XSetLineAttributes(st->dpy, st->text_fg_gc, 0, 0, 0, 0);
    XDrawRectangle(st->dpy, st->window, st->text_fg_gc,
	 st->s_rect.x + 7, st->s_rect.y + 7, st->s_rect.width - 15, st->s_rect.height - 15);

    st->X = 15;
    st->Y = 15 + font_height(st->font);

    /* now print each string in reverse order (start at bottom of box) */
    for (Z = 0; Z < height; Z++)
    {
        int L = strlen(args[Z]);
        if (args[Z][L-1] == '\r' || args[Z][L-1] == '\n')
          args[Z][--L] = 0;
	XDrawString(st->dpy, st->window, st->text_fg_gc, st->s_rect.x + st->X, st->s_rect.y + st->Y,
		    args[Z], L);
	st->Y += font_height(st->font);
    }
    st->interval = (total / 15) * 1000;
    if (st->interval < 2000) st->interval = 2000;
    st->next_fn = talk_1;
    *st->words = 0;
    st->lines = 0;
}

static void
talk_1 (struct state *st) 
{
  talk(st, 0);
}


static unsigned long
look (struct state *st)
{
    if (random() % 3)
    {
	COPY(st->dpy, (random() & 1) ? st->down : st->front, st->window, st->fg_gc,
	     0, 0, 64, 64, st->x, st->y);
	return 1000L;
    }
    if (!(random() % 5))
	return 0;
    if (random() % 3)
    {
	COPY(st->dpy, (random() & 1) ? st->left_front : st->right_front,
	     st->window, st->fg_gc, 0, 0, 64, 64, st->x, st->y);
	return 1000L;
    }
    if (!(random() % 5))
	return 0;
    COPY(st->dpy, (random() & 1) ? st->left1 : st->right1, st->window, st->fg_gc,
	 0, 0, 64, 64, st->x, st->y);
    return 1000L;
}


static void
fill_words (struct state *st)
{
  char *p = st->words + strlen(st->words);
  while (p < st->words + sizeof(st->words) - 1 &&
         st->lines < MAXLINES)
    {
      char c = textclient_getc (st->tc);
      if (c == '\n')
        st->lines++;
      if (c > 0)
        *p++ = c;
      else
        break;
    }
  *p = 0;
}



static const char *noseguy_defaults [] = {
  ".background:	    black",
  ".foreground:	    #CCCCCC",
  "*textForeground: black",
  "*textBackground: #CCCCCC",
  "*fpsSolid:	 true",
  "*program:	 xscreensaver-text --cols 40 | head -n15",
  "*usePty:      False",
  ".font:	 -*-new century schoolbook-*-r-*-*-*-180-*-*-*-*-*-*",
  0
};

static XrmOptionDescRec noseguy_options [] = {
  { "-program",		".program",		XrmoptionSepArg, 0 },
  { "-font",		".font",		XrmoptionSepArg, 0 },
  { "-text-foreground",	".textForeground",	XrmoptionSepArg, 0 },
  { "-text-background",	".textBackground",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


static void *
noseguy_init (Display *d, Window w)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  unsigned long fg, bg, text_fg, text_bg;
  XWindowAttributes xgwa;
  Colormap cmap;
  char *fontname;
  XGCValues gcvalues;
  st->dpy = d;
  st->window = w;
  st->first_time = 1;

  fontname = get_string_resource (st->dpy, "font", "Font");
  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  st->Width = xgwa.width + 2;
  st->Height = xgwa.height + 2;
  cmap = xgwa.colormap;

  st->program = get_string_resource (st->dpy, "program", "Program");
  st->tc = textclient_open (st->dpy);
  init_images(st);

  if (!fontname || !*fontname)
    fprintf (stderr, "%s: no font specified.\n", progname);
  st->font = XLoadQueryFont(st->dpy, fontname);
  if (!st->font) {
    fprintf (stderr, "%s: could not load font %s.\n", progname, fontname);
    exit(1);
  }

  fg = get_pixel_resource (st->dpy, cmap, "foreground", "Foreground");
  bg = get_pixel_resource (st->dpy, cmap, "background", "Background");
  text_fg = get_pixel_resource (st->dpy, cmap, "textForeground", "Foreground");
  text_bg = get_pixel_resource (st->dpy, cmap, "textBackground", "Background");
  /* notice when unspecified */
  if (! get_string_resource (st->dpy, "textForeground", "Foreground"))
    text_fg = bg;
  if (! get_string_resource (st->dpy, "textBackground", "Background"))
    text_bg = fg;

  gcvalues.font = st->font->fid;
  gcvalues.foreground = fg;
  gcvalues.background = bg;
  st->fg_gc = XCreateGC (st->dpy, st->window,
                         GCForeground|GCBackground|GCFont,
		     &gcvalues);
  gcvalues.foreground = bg;
  gcvalues.background = fg;
  st->bg_gc = XCreateGC (st->dpy, st->window,
                         GCForeground|GCBackground|GCFont,
		     &gcvalues);
  gcvalues.foreground = text_fg;
  gcvalues.background = text_bg;
  st->text_fg_gc = XCreateGC (st->dpy, st->window,
                              GCForeground|GCBackground|GCFont,
			  &gcvalues);
  gcvalues.foreground = text_bg;
  gcvalues.background = text_fg;
  st->text_bg_gc = XCreateGC (st->dpy, st->window, 
                              GCForeground|GCBackground|GCFont,
			  &gcvalues);
  st->x = st->Width / 2;
  st->y = st->Height / 2;
  st->state = IS_MOVING;
  st->next_fn = move;
  st->walk_up = 1;
  return st;
}
     
static unsigned long
noseguy_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  fill_words(st);
  st->next_fn(st);
  return (st->interval * 1000);
}

static void
noseguy_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
}

static Bool
noseguy_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
noseguy_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  textclient_close (st->tc);
  free (st);
}

XSCREENSAVER_MODULE ("NoseGuy", noseguy)
