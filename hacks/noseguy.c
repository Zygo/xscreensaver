/* xscreensaver, Copyright (c) 1992, 1996, 1997, 1998
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

/* Make a little guy with a big nose and a hat wanter around the screen,
   spewing out messages.  Derived from xnlock by Dan Heller <argv@sun.com>.
 */

#include "screenhack.h"
#include <stdio.h>

extern FILE *popen (const char *, const char *);
extern int pclose (FILE *);

#define font_height(font)	  	(font->ascent + font->descent)
#define FONT_NAME			"-*-times-*-*-*-*-18-*-*-*-*-*-*-*"

static Display *dpy;
static Window window;
static int Width, Height;
static GC fg_gc, bg_gc, text_fg_gc, text_bg_gc;
static char *words;
static char *get_words (void);
static int x, y;
static XFontStruct *font;
static char *def_words = "I'm out running around.";
static void walk (int dir);
static void talk (int erase);
static void talk_1 (void);
static int think (void);
static unsigned long interval;
static unsigned long look (void); 
static Pixmap left1, left2, right1, right2;
static Pixmap left_front, right_front, front, down;

static char *program, *orig_program, *filename, *text;

#define FROM_ARGV    1
#define FROM_PROGRAM 2
#define FROM_FILE    3
#define FROM_RESRC   4
static int getwordsfrom;

#define IS_MOVING  1
#define GET_PASSWD 2
static int state;	/* indicates states: walking or getting passwd */

static void (*next_fn) (void);

#ifdef HAVE_XPM
# include <X11/xpm.h>

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
init_images (void)
{
  static Pixmap *images[] = {
    &left1, &left2, &right1, &right2,
    &left_front, &right_front, &front, &down
  };
  int i;
#ifdef HAVE_XPM
  static char **bits[] = {
    nose_l1_xpm, nose_l2_xpm, nose_r1_xpm, nose_r2_xpm,
    nose_f2_xpm, nose_f3_xpm, nose_f1_xpm, nose_f4_xpm
  };
  for (i = 0; i < sizeof (images) / sizeof(*images); i++)
    {
      XWindowAttributes xgwa;
      XpmAttributes xpmattrs;
      Pixmap pixmap = 0;
      int result;
      xpmattrs.valuemask = 0;

      XGetWindowAttributes (dpy, window, &xgwa);

# ifdef XpmCloseness
      xpmattrs.valuemask |= XpmCloseness;
      xpmattrs.closeness = 40000;
# endif
# ifdef XpmVisual
      xpmattrs.valuemask |= XpmVisual;
      xpmattrs.visual = xgwa.visual;
# endif
# ifdef XpmDepth
      xpmattrs.valuemask |= XpmDepth;
      xpmattrs.depth = xgwa.depth;
# endif
# ifdef XpmColormap
      xpmattrs.valuemask |= XpmColormap;
      xpmattrs.colormap = xgwa.colormap;
# endif

      result = XpmCreatePixmapFromData(dpy, window, bits[i],
				       &pixmap, 0 /* mask */, &xpmattrs);
      if (!pixmap || (result != XpmSuccess && result != XpmColorError))
	{
	  fprintf (stderr, "%s: Can't load nose images\n", progname);
	  exit (1);
	}
      *images[i] = pixmap;
    }
#else
  static unsigned char *bits[] = {
    nose_l1_bits, nose_l2_bits, nose_r1_bits, nose_r2_bits,
    nose_f2_bits, nose_f3_bits, nose_f1_bits, nose_f4_bits
  };

  for (i = 0; i < sizeof (images) / sizeof(*images); i++)
    if (!(*images[i] =
	  XCreatePixmapFromBitmapData(dpy, window,
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
move (void)
{
    static int      length,
                    dir;

    if (!length)
    {
	register int    tries = 0;
	dir = 0;
	if ((random() & 1) && think())
	{
	    talk(0);		/* sets timeout to itself */
	    return;
	}
	if (!(random() % 3) && (interval = look()))
	{
	    next_fn = move;
	    return;
	}
	interval = 20 + random() % 100;
	do
	{
	    if (!tries)
		length = Width / 100 + random() % 90, tries = 8;
	    else
		tries--;
	    switch (random() % 8)
	    {
	    case 0:
		if (x - X_INCR * length >= 5)
		    dir = LEFT;
		break;
	    case 1:
		if (x + X_INCR * length <= Width - 70)
		    dir = RIGHT;
		break;
	    case 2:
		if (y - (Y_INCR * length) >= 5)
		    dir = UP, interval = 40;
		break;
	    case 3:
		if (y + Y_INCR * length <= Height - 70)
		    dir = DOWN, interval = 20;
		break;
	    case 4:
		if (x - X_INCR * length >= 5 && y - (Y_INCR * length) >= 5)
		    dir = (LEFT | UP);
		break;
	    case 5:
		if (x + X_INCR * length <= Width - 70 &&
		    y - Y_INCR * length >= 5)
		    dir = (RIGHT | UP);
		break;
	    case 6:
		if (x - X_INCR * length >= 5 &&
		    y + Y_INCR * length <= Height - 70)
		    dir = (LEFT | DOWN);
		break;
	    case 7:
		if (x + X_INCR * length <= Width - 70 &&
		    y + Y_INCR * length <= Height - 70)
		    dir = (RIGHT | DOWN);
		break;
	    default:
		/* No Defaults */
		break;
	    }
	} while (!dir);
    }
    walk(dir);
    --length;
    next_fn = move;
}

#ifdef HAVE_XPM
# define COPY(dpy,frame,window,gc,x,y,w,h,x2,y2) \
  XCopyArea (dpy,frame,window,gc,x,y,w,h,x2,y2)
#else
# define COPY(dpy,frame,window,gc,x,y,w,h,x2,y2) \
  XCopyPlane(dpy,frame,window,gc,x,y,w,h,x2,y2,1L)
#endif

static void
walk(int dir)
{
    register int    incr = 0;
    static int      lastdir;
    static int      up = 1;
    static Pixmap   frame;

    if (dir & (LEFT | RIGHT))
    {				/* left/right movement (mabye up/down too) */
	up = -up;		/* bouncing effect (even if hit a wall) */
	if (dir & LEFT)
	{
	    incr = X_INCR;
	    frame = (up < 0) ? left1 : left2;
	}
	else
	{
	    incr = -X_INCR;
	    frame = (up < 0) ? right1 : right2;
	}
	if ((lastdir == FRONT || lastdir == DOWN) && dir & UP)
	{

	    /*
	     * workaround silly bug that leaves screen dust when guy is
	     * facing forward or down and moves up-left/right.
	     */
	    COPY(dpy, frame, window, fg_gc, 0, 0, 64, 64, x, y);
	    XFlush(dpy);
	}
	/* note that maybe neither UP nor DOWN is set! */
	if (dir & UP && y > Y_INCR)
	    y -= Y_INCR;
	else if (dir & DOWN && y < Height - 64)
	    y += Y_INCR;
    }
    /* Explicit up/down movement only (no left/right) */
    else if (dir == UP)
	COPY(dpy, front, window, fg_gc, 0, 0, 64, 64, x, y -= Y_INCR);
    else if (dir == DOWN)
	COPY(dpy, down, window, fg_gc, 0, 0, 64, 64, x, y += Y_INCR);
    else if (dir == FRONT && frame != front)
    {
	if (up > 0)
	    up = -up;
	if (lastdir & LEFT)
	    frame = left_front;
	else if (lastdir & RIGHT)
	    frame = right_front;
	else
	    frame = front;
	COPY(dpy, frame, window, fg_gc, 0, 0, 64, 64, x, y);
    }
    if (dir & LEFT)
	while (--incr >= 0)
	{
	    COPY(dpy, frame, window, fg_gc, 0, 0, 64, 64, --x, y + up);
	    XFlush(dpy);
	}
    else if (dir & RIGHT)
	while (++incr <= 0)
	{
	    COPY(dpy, frame, window, fg_gc, 0, 0, 64, 64, ++x, y + up);
	    XFlush(dpy);
	}
    lastdir = dir;
}

static int
think (void)
{
    if (random() & 1)
	walk(FRONT);
    if (random() & 1)
    {
	if (getwordsfrom == FROM_PROGRAM)
	    words = get_words();
	return 1;
    }
    return 0;
}

#define MAXLINES 40

static void
talk(int force_erase)
{
    int             width = 0,
                    height,
                    Z,
                    total = 0;
    static int      X,
                    Y,
                    talking;
    static struct
    {
	int             x,
	                y,
	                width,
	                height;
    }               s_rect;
    register char  *p,
                   *p2;
    char            buf[BUFSIZ],
                    args[MAXLINES][256];

    /* clear what we've written */
    if (talking || force_erase)
    {
	if (!talking)
	    return;
	XFillRectangle(dpy, window, bg_gc, s_rect.x - 5, s_rect.y - 5,
		       s_rect.width + 10, s_rect.height + 10);
	talking = 0;
	if (!force_erase)
	  next_fn = move;
	interval = 0;
	{
	  /* might as well check the window for size changes now... */
	  XWindowAttributes xgwa;
	  XGetWindowAttributes (dpy, window, &xgwa);
	  Width = xgwa.width + 2;
	  Height = xgwa.height + 2;
	}
	return;
    }
    talking = 1;
    walk(FRONT);
    p = strcpy(buf, words);

    if (!(p2 = strchr(p, '\n')) || !p2[1])
      {
	total = strlen (words);
	strcpy (args[0], words);
	width = XTextWidth(font, words, total);
	height = 0;
      }
    else
      /* p2 now points to the first '\n' */
      for (height = 0; p; height++)
	{
	  int             w;
	  *p2 = 0;
	  if ((w = XTextWidth(font, p, p2 - p)) > width)
	    width = w;
	  total += p2 - p;	/* total chars; count to determine reading
				 * time */
	  (void) strcpy(args[height], p);
	  if (height == MAXLINES - 1)
	    {
	      puts("Message too long!");
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
    s_rect.width = width + 30;
    s_rect.height = height * font_height(font) + 30;
    if (x - s_rect.width - 10 < 5)
	s_rect.x = 5;
    else if ((s_rect.x = x + 32 - (s_rect.width + 15) / 2)
	     + s_rect.width + 15 > Width - 5)
	s_rect.x = Width - 15 - s_rect.width;
    if (y - s_rect.height - 10 < 5)
	s_rect.y = y + 64 + 5;
    else
	s_rect.y = y - 5 - s_rect.height;

    XFillRectangle(dpy, window, text_bg_gc,
	 s_rect.x, s_rect.y, s_rect.width, s_rect.height);

    /* make a box that's 5 pixels thick. Then add a thin box inside it */
    XSetLineAttributes(dpy, text_fg_gc, 5, 0, 0, 0);
    XDrawRectangle(dpy, window, text_fg_gc,
		   s_rect.x, s_rect.y, s_rect.width - 1, s_rect.height - 1);
    XSetLineAttributes(dpy, text_fg_gc, 0, 0, 0, 0);
    XDrawRectangle(dpy, window, text_fg_gc,
	 s_rect.x + 7, s_rect.y + 7, s_rect.width - 15, s_rect.height - 15);

    X = 15;
    Y = 15 + font_height(font);

    /* now print each string in reverse order (start at bottom of box) */
    for (Z = 0; Z < height; Z++)
    {
	XDrawString(dpy, window, text_fg_gc, s_rect.x + X, s_rect.y + Y,
		    args[Z], strlen(args[Z]));
	Y += font_height(font);
    }
    interval = (total / 15) * 1000;
    if (interval < 2000) interval = 2000;
    next_fn = talk_1;
}

static void talk_1 (void) 
{
  talk(0);
}


static unsigned long
look (void)
{
    if (random() % 3)
    {
	COPY(dpy, (random() & 1) ? down : front, window, fg_gc,
	     0, 0, 64, 64, x, y);
	return 1000L;
    }
    if (!(random() % 5))
	return 0;
    if (random() % 3)
    {
	COPY(dpy, (random() & 1) ? left_front : right_front,
	     window, fg_gc, 0, 0, 64, 64, x, y);
	return 1000L;
    }
    if (!(random() % 5))
	return 0;
    COPY(dpy, (random() & 1) ? left1 : right1, window, fg_gc,
	 0, 0, 64, 64, x, y);
    return 1000L;
}


static void
init_words (void)
{
  char *mode = get_string_resource ("mode", "Mode");

  program = get_string_resource ("program", "Program");
  filename = get_string_resource ("filename", "Filename");
  text = get_string_resource ("text", "Text");

  if (program)	/* get stderr on stdout, so it shows up on the window */
    {
      orig_program = program;
      program = (char *) malloc (strlen (program) + 10);
      strcpy (program, "( ");
      strcat (program, orig_program);
      strcat (program, " ) 2>&1");
    }

  if (!mode || !strcmp (mode, "program"))
    getwordsfrom = FROM_PROGRAM;
  else if (!strcmp (mode, "file"))
    getwordsfrom = FROM_FILE;
  else if (!strcmp (mode, "string"))
    getwordsfrom = FROM_RESRC;
  else
    {
      fprintf (stderr,
	       "%s: mode must be program, file, or string, not %s\n",
	       progname, mode);
      exit (1);
    }

  if (getwordsfrom == FROM_PROGRAM && !program)
    {
      fprintf (stderr, "%s: no program specified.\n", progname);
      exit (1);
    }
  if (getwordsfrom == FROM_FILE && !filename)
    {
      fprintf (stderr, "%s: no file specified.\n", progname);
      exit (1);
    }

  words = get_words();	
}

static int first_time = 1;

static char *
get_words (void)
{
    FILE           *pp;
    static char     buf[BUFSIZ];
    register char  *p = buf;

    buf[0] = '\0';

    switch (getwordsfrom)
    {
    case FROM_PROGRAM:
#ifndef VMS
	if ((pp = popen(program, "r")))
	{
	    while (fgets(p, sizeof(buf) - strlen(buf), pp))
	    {
		if (strlen(buf) + 1 < sizeof(buf))
		    p = buf + strlen(buf);
		else
		    break;
	    }
	    (void) pclose(pp);
	    if (! buf[0])
	      sprintf (buf, "\"%s\" produced no output!", orig_program);
	    else if (!first_time &&
		     (strstr (buf, ": not found") ||
		      strstr (buf, ": Not found")))
	      switch (random () % 20)
		{
		case 1: strcat (buf, "( Get with the program, bub. )\n");
		  break;
		case 2: strcat (buf,
		  "( I blow my nose at you, you silly person! ) \n"); break;
		case 3: strcat (buf,
		  "\nThe resource you want to\nset is `noseguy.program'\n");
		  break;
		case 4:
		  strcat(buf,"\nHelp!!  Help!!\nAAAAAAGGGGHHH!!  \n\n"); break;
		case 5: strcpy (buf, "You have new mail.\n"); break;
		case 6:
		  strcat(buf,"( Hello?  Are you paying attention? )\n");break;
		case 7:
		  strcat (buf, "sh: what kind of fool do you take me for? \n");
		  break;
		}
	    first_time = 0;
	    p = buf;
	}
	else
	{
	    perror(program);
	    p = def_words;
	}
	break;
#endif /* VMS */
    case FROM_FILE:
	if ((pp = fopen(filename, "r")))
	{
	    while (fgets(p, sizeof(buf) - strlen(buf), pp))
	    {
		if (strlen(buf) + 1 < sizeof(buf))
		    p = buf + strlen(buf);
		else
		    break;
	    }
	    (void) fclose(pp);
	    if (! buf[0])
	      sprintf (buf, "file \"%s\" is empty!", filename);
	    p = buf;
	}
	else
	{
	  sprintf (buf, "couldn't read file \"%s\"!", filename);
	  p = buf;
	}
	break;
    case FROM_RESRC:
	p = text;
	break;
    default:
	p = def_words;
	break;
    }

    if (!p || *p == '\0')
	p = def_words;
    return p;
}



char *progclass = "Noseguy";

char *defaults [] = {
  ".background:		black",
  ".foreground:		gray80",
#ifndef VMS
  "*mode:		program",
#else
  "*mode:		string",
#endif
  "*program:		" ZIPPY_PROGRAM,
  "noseguy.font:	-*-new century schoolbook-*-r-*-*-*-180-*-*-*-*-*-*",
  0
};

XrmOptionDescRec options [] = {
  { "-mode",		".mode",		XrmoptionSepArg, 0 },
  { "-program",		".program",		XrmoptionSepArg, 0 },
  { "-text",		".text",		XrmoptionSepArg, 0 },
  { "-filename",	".filename",		XrmoptionSepArg, 0 },
  { "-font",		".font",		XrmoptionSepArg, 0 },
  { "-text-foreground",	".textForeground",	XrmoptionSepArg, 0 },
  { "-text-background",	".textBackground",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};


static void
noseguy_init (Display *d, Window w)
{
  unsigned long fg, bg, text_fg, text_bg;
  XWindowAttributes xgwa;
  Colormap cmap;
  char *fontname = get_string_resource ("font", "Font");
  char **list;
  int foo, i;
  XGCValues gcvalues;
  dpy = d;
  window = w;
  XGetWindowAttributes (dpy, window, &xgwa);
  Width = xgwa.width + 2;
  Height = xgwa.height + 2;
  cmap = xgwa.colormap;

  init_words();
  init_images();

  if (!fontname || !(font = XLoadQueryFont(dpy, fontname)))
    {
	list = XListFonts(dpy, FONT_NAME, 32767, &foo);
	for (i = 0; i < foo; i++)
	    if ((font = XLoadQueryFont(dpy, list[i])))
		break;
	if (!font)
	  {
	    fprintf (stderr, "%s: Can't find a large font.", progname);
	    exit (1);
	  }
	XFreeFontNames(list);
    }

  fg = get_pixel_resource ("foreground", "Foreground", dpy, cmap);
  bg = get_pixel_resource ("background", "Background", dpy, cmap);
  text_fg = get_pixel_resource ("textForeground", "Foreground", dpy, cmap);
  text_bg = get_pixel_resource ("textBackground", "Background", dpy, cmap);
  /* notice when unspecified */
  if (! get_string_resource ("textForeground", "Foreground"))
    text_fg = bg;
  if (! get_string_resource ("textBackground", "Background"))
    text_bg = fg;

  gcvalues.font = font->fid;
  gcvalues.graphics_exposures = False;
  gcvalues.foreground = fg;
  gcvalues.background = bg;
  fg_gc = XCreateGC (dpy, window,
		     GCForeground|GCBackground|GCGraphicsExposures|GCFont,
		     &gcvalues);
  gcvalues.foreground = bg;
  gcvalues.background = fg;
  bg_gc = XCreateGC (dpy, window,
		     GCForeground|GCBackground|GCGraphicsExposures|GCFont,
		     &gcvalues);
  gcvalues.foreground = text_fg;
  gcvalues.background = text_bg;
  text_fg_gc = XCreateGC (dpy, window,
			  GCForeground|GCBackground|GCGraphicsExposures|GCFont,
			  &gcvalues);
  gcvalues.foreground = text_bg;
  gcvalues.background = text_fg;
  text_bg_gc = XCreateGC (dpy, window,
			  GCForeground|GCBackground|GCGraphicsExposures|GCFont,
			  &gcvalues);
  x = Width / 2;
  y = Height / 2;
  state = IS_MOVING;
}
     
void
screenhack (Display *d, Window w)
{
  noseguy_init (d, w);
  next_fn = move;
  while (1)
    {
      next_fn();
      XSync (dpy, True);
      usleep (interval * 1000);
    }
}

