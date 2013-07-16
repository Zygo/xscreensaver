/* xscreensaver, Copyright (c) 1997-2013 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Wendy, let me explain something to you.  Whenever you come in here and
 * interrupt me, you're BREAKING my CONCENTRATION.  You're DISTRACTING me!
 * And it will then take me time to get back to where I was. You understand?
 * Now, we're going to make a new rule.  When you come in here and you hear
 * me typing, or whether you DON'T hear me typing, or whatever the FUCK you
 * hear me doing; when I'm in here, it means that I am working, THAT means
 * don't come in!  Now, do you think you can handle that?
 */

#include <ctype.h>
#include "screenhack.h"

static const char *source = "All work and no play makes Jack a dull boy.  ";
/* If you're here because you're thinking about making the above string be
   customizable, then you don't get the joke.  You loser. */

struct state {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  XFontStruct *font;
  GC gc;

  const char *s;
  int columns, rows;		/* characters */
  int left, right;		/* characters */
  int char_width, line_height;	/* pixels */
  int x, y;			/* characters */
  int mode;
  int hspace;			/* pixels */
  int vspace;			/* pixels */
  Bool break_para;
  Bool caps;
  int sentences;
  int paras;
  int scrolling;
  int subscrolling;
  int pining;

  int delay;
};


static void
xjack_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  st->columns = (st->xgwa.width  - st->hspace - st->hspace) / st->char_width;
  st->rows    = (st->xgwa.height - st->vspace - st->vspace) / st->line_height;
  st->rows--;
  st->columns--;

  /* If the window is stupidly small, just truncate. */
  if (st->rows < 4)     st->rows = 4;
  if (st->columns < 12) st->columns = 12;

  if (st->y > st->rows)    st->y = st->rows-1;
  if (st->x > st->columns) st->x = st->columns-2;

  if (st->right > st->columns) st->right = st->columns;
  if (st->left > st->columns-20) st->left = st->columns-20;
  if (st->left < 0) st->left = 0;

  XClearWindow (st->dpy, st->window);
}


static void *
xjack_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  char *fontname;

  st->dpy = dpy;
  st->window = window;
  st->s = source;
  st->delay = get_integer_resource (st->dpy, "delay", "Integer");
  fontname = get_string_resource (st->dpy, "font", "Font");

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  if (st->xgwa.width <= 480)
    fontname = "-*-courier-medium-r-*-*-*-180-*-*-m-*-*-*";

  st->font = XLoadQueryFont (st->dpy, fontname);

  if (!st->font)
    st->font = XLoadQueryFont (st->dpy, "-*-*-medium-r-*-*-*-240-*-*-m-*-*-*");
  if (!st->font)
    st->font = XLoadQueryFont (st->dpy,
                               "-*-courier-medium-r-*-*-*-180-*-*-m-*-*-*");
  if (!st->font)
    st->font = XLoadQueryFont (st->dpy, "-*-*-*-r-*-*-*-240-*-*-m-*-*-*");
  if (!st->font)
    {
      fprintf(stderr, "no big fixed-width font like \"%s\"\n", fontname);
      exit(1);
    }

  gcv.font = st->font->fid;
  gcv.foreground = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                       "foreground", "Foreground");
  gcv.background = get_pixel_resource (st->dpy, st->xgwa.colormap,
                                       "background", "Background");
  st->gc = XCreateGC (st->dpy, st->window,
                      (GCFont | GCForeground | GCBackground), &gcv);

  st->char_width = 
    (st->font->per_char
     ? st->font->per_char['n'-st->font->min_char_or_byte2].rbearing
     : st->font->min_bounds.rbearing);
  st->line_height = st->font->ascent + st->font->descent + 1;

  xjack_reshape (dpy, window, st, st->xgwa.width, st->xgwa.height);

  st->left = 0xFF & (random() % ((st->columns / 2)+1));
  st->right = st->left + (0xFF & (random() % (st->columns - st->left)
                                  + 10));
  if (st->right < st->left + 10) st->right = st->left + 10;
  if (st->right > st->columns)   st->right = st->columns;

  st->x = st->left;
  st->y = 0;

  if (st->xgwa.width > 200 && st->xgwa.height > 200)
    st->hspace = st->vspace = 40;

  return st;
}

static unsigned long
xjack_scroll (struct state *st)
{
  st->break_para = 0;
  if (st->subscrolling)
    {
      int inc = st->line_height / 7;
      XCopyArea (st->dpy, st->window, st->window, st->gc,
                 0, inc,
                 st->xgwa.width, st->xgwa.height - inc,
                 0, 0);

      /* See? It's OK. He saw it on the television. */
      XClearArea (st->dpy, st->window,
                  0, st->xgwa.height - inc, st->xgwa.width, inc,
                  False);

      st->subscrolling -= inc;
      if (st->subscrolling <= 0)
        st->subscrolling = 0;
      if (st->subscrolling == 0)
        {
          if (st->scrolling > 0)
            st->scrolling--;
          st->y--;
        }
      return st->delay / 1000;
    }
  else if (st->scrolling)
    st->subscrolling = st->line_height;

  if (st->y < 0)
    st->y = 0;
  else if (st->y >= st->rows-1)
    st->y = st->rows-1;

  return st->delay;
}

static unsigned long
xjack_pine (struct state *st)
{
  /* See also http://catalog.com/hopkins/unix-haters/login.html */
  const char *n1 = "NFS server overlook not responding, still trying...";
  const char *n2 = "NFS server overlook ok.";
  int prev = st->pining;

  if (!st->pining)
    st->pining = 1 + (random() % 3);

  if (prev)
    while (*n2)
      {
        XDrawString (st->dpy, st->window, st->gc,
                     (st->x * st->char_width) + st->hspace,
                     ((st->y * st->line_height) + st->vspace
                      + st->font->ascent),
                     (char *) n2, 1);
        st->x++;
        if (st->x >= st->columns) st->x = 0, st->y++;
        n2++;
      }
  st->y++;
  st->x = 0;
  st->pining--;

  if (st->pining)
    while (*n1)
      {
        XDrawString (st->dpy, st->window, st->gc,
                     (st->x * st->char_width) + st->hspace,
                     ((st->y * st->line_height) + st->vspace
                      + st->font->ascent),
                     (char *) n1, 1);
        st->x++;
        if (st->x >= st->columns) st->x = 0, st->y++;
        n1++;
      }

  return 5000000;
}


static unsigned long
xjack_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  int this_delay = st->delay;
  int word_length = 0;
  const char *s2;

  if (st->scrolling)
    return xjack_scroll (st);
  if (st->pining)
    return xjack_pine (st);

  for (s2 = st->s; *s2 && *s2 != ' '; s2++)
    word_length++;

  if (st->break_para ||
      (*st->s != ' ' &&
       (st->x + word_length) >= st->right))
    {
      st->x = st->left;
      st->y++;

      if (st->break_para)
        st->y++;

      st->break_para = 0;

      if (st->mode == 1 || st->mode == 2)
        {
          /* 1 = left margin goes southwest; 2 = southeast */
          st->left += (st->mode == 1 ? 1 : -1);
          if (st->left >= st->right - 10)
            {
              if ((st->right < (st->columns - 10)) && (random() & 1))
                st->right += (0xFF & (random() % (st->columns - st->right)));
              else
                st->mode = 2;
            }
          else if (st->left <= 0)
            {
              st->left = 0;
              st->mode = 1;
            }
        }
      else if (st->mode == 3 || st->mode == 4)
        {
          /* 3 = right margin goes southeast; 4 = southwest */
          st->right += (st->mode == 3 ? 1 : -1);
          if (st->right >= st->columns)
            {
              st->right = st->columns;
              st->mode = 4;
            }
          else if (st->right <= st->left + 10)
            st->mode = 3;
        }

      if (st->y >= st->rows-1)	/* bottom of page */
        {
# if 0    /* Nah, this looks bad. */

          /* scroll by 1-5 lines */
          st->scrolling = (random() % 5 ? 0 : (0xFF & (random() % 5))) + 1;

          if (st->break_para)
            st->scrolling++;

          /* but sometimes scroll by a whole page */
          if (0 == (random() % 100))
            st->scrolling += st->rows;
# else
          st->scrolling = 1;
# endif

          return xjack_scroll (st);
        }
    }

  if (*st->s != ' ')
    {
      char c = *st->s;
      int xshift = 0, yshift = 0;
      if (0 == random() % 50)
        {
          xshift = random() % ((st->char_width / 3) + 1);      /* mis-strike */
          yshift = random() % ((st->line_height / 6) + 1);
          if (0 == (random() % 3))
            yshift *= 2;
          if (random() & 1)
            xshift = -xshift;
          if (random() & 1)
            yshift = -yshift;
        }

      if (0 == (random() % 250))	/* introduce adjascent-key typo */
        {
          static const char * const typo[] = {
            "asqw", "ASQW", "bgvhn", "cxdfv", "dserfcx", "ewsdrf",
            "Jhuikmn", "kjiol,m", "lkop;.,", "mnjk,", "nbhjm", "oiklp09",
            "pol;(-0", "redft54", "sawedxz", "uyhji87", "wqase32",
            "yuhgt67", ".,l;/", 0 };
          int i = 0;
          while (typo[i] && typo[i][0] != c)
            i++;
          if (typo[i])
            c = typo[i][0xFF & ((random() % strlen(typo[i]+1)) + 1)];
        }

      /* caps typo */
      if (c >= 'a' && c <= 'z' && (st->caps || 0 == (random() % 350)))
        {
          c -= ('a'-'A');
          if (c == 'O' && random() & 1)
            c = '0';
        }

    OVERSTRIKE:
      XDrawString (st->dpy, st->window, st->gc,
                   (st->x * st->char_width) + st->hspace + xshift,
                   ((st->y * st->line_height) + st->vspace + st->font->ascent
                    + yshift),
                   &c, 1);
      if (xshift == 0 && yshift == 0 && (0 == (random() & 3000)))
        {
          if (random() & 1)
            xshift--;
          else
            yshift--;
          goto OVERSTRIKE;
        }

      if ((tolower(c) != tolower(*st->s))
          ? (0 == (random() % 10))	/* backup to correct */
          : (0 == (random() % 400)))	/* fail to advance */
        {
          st->x--;
          st->s--;
          if (st->delay)
            st->delay += (0xFFFF & (st->delay + 
                                    (random() % (st->delay * 10))));
        }
    }

  st->x++;
  st->s++;

  if (0 == random() % 200)
    {
      if (random() & 1 && st->s != source)
        st->s--;	/* duplicate character */
      else if (*st->s)
        st->s++;	/* skip character */
    }

  if (*st->s == 0)
    {
      st->sentences++;
      st->caps = (0 == random() % 40);	/* capitalize sentence */

      if (0 == (random() % 10) ||	/* randomly break paragraph */
          (st->mode == 0 &&
           ((0 == (random() % 10)) || st->sentences > 20)))
        {
          st->break_para = True;
          st->sentences = 0;
          st->paras++;

          if (random() & 1)		/* mode=0 50% of the time */
            st->mode = 0;
          else
            st->mode = (0xFF & (random() % 5));

          if (0 == (random() % 2))	/* re-pick margins */
            {
              st->left = 0xFF & (random() % (st->columns / 3));
              st->right = (st->columns -
                           (0xFF & (random() % (st->columns / 3))));

              if (0 == random() % 3)	/* sometimes be wide */
                st->right = st->left + ((st->right - st->left) / 2);
            }

          if (st->right - st->left <= 10)	/* introduce sanity */
            {
              st->left = 0;
              st->right = st->columns;
            }

          if (st->right - st->left > 50)	/* if wide, shrink and move */
            {
              st->left += (0xFF & (random() % ((st->columns - 50) + 1)));
              st->right = st->left + (0xFF & ((random() % 40) + 10));
            }

          /* oh, gag. */
          if (st->mode == 0 &&
              st->right - st->left < 25 &&
              st->columns > 40)
            {
              st->right += 20;
              if (st->right > st->columns)
                st->left -= (st->right - st->columns);
            }

          if (st->right - st->left < 5)
            st->left = st->right - 5;
          if (st->left < 0)
            st->left = 0;
          if (st->right - st->left < 5)
            st->right = st->left + 5;
        }
      st->s = source;
    }

  if (st->delay)
    {
      if (0 == random() % 3)
        this_delay += (0xFFFFFF & ((random() % (st->delay * 5)) + 1));

      if (st->break_para)
        this_delay += (0xFFFFFF & ((random() % (st->delay * 15)) + 1));
    }

  if (st->paras > 5 &&
      (0 == (random() % 1000)) &&
      st->y < st->rows-2)
    return xjack_pine (st);

  return this_delay;
}

static Bool
xjack_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (event->xany.type == ButtonPress)
    {
      st->scrolling++;
      return True;
    }

  return False;
}

static void
xjack_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  free (st);
}


static const char *xjack_defaults [] = {
  ".background:		#FFF0B4",
  ".foreground:		#000000",
  "*fpsSolid:		true",
#ifdef HAVE_COCOA
  ".font:		American Typewriter 24",
#else
  ".font:		-*-courier-medium-r-*-*-*-240-*-*-m-*-*-*",
#endif
  "*delay:		50000",
  0
};

static XrmOptionDescRec xjack_options [] = {
  { "-delay",		".delay",	XrmoptionSepArg, 0 },
  { "-font",		".font",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("XJack", xjack)
