/* xscreensaver, Copyright (c) 2002, 2004 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Memscroller -- scrolls a dump of its own RAM across the screen.
 */

#include "screenhack.h"
#include <stdio.h>
#include <X11/Xutil.h>

#ifdef HAVE_XSHM_EXTENSION
#include "xshm.h"
#endif

typedef struct {
  int which;
  XRectangle rect;
  XImage *image;
  int rez;
  int speed;
  int scroll_tick;
  unsigned char *data;
  int count_zero;
} scroller;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  GC draw_gc, erase_gc, text_gc;
  XFontStruct *font;
  int border;

  enum { SEED_RAM, SEED_RANDOM, SEED_FILE } seed_mode;
  enum { DRAW_COLOR, DRAW_MONO } draw_mode;

  char *filename;
  FILE *in;

  int nscrollers;
  scroller *scrollers;

# ifdef HAVE_XSHM_EXTENSION
  Bool shm_p;
  XShmSegmentInfo shm_info;
# endif

} state;


state *
init_memscroller (Display *dpy, Window window)
{
  int i;
  XGCValues gcv;
  state *st = (state *) calloc (1, sizeof (*st));
  char *s;
  st->dpy = dpy;
  st->window = window;
  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);
  XSelectInput(st->dpy, st->window, ExposureMask);

  /* Fill up the colormap with random colors.
     We don't actually use these explicitly, but in 8-bit mode,
     they will be used implicitly by the random image bits. */
  {
    int ncolors = 255;
    XColor colors[256];
    make_random_colormap (st->dpy, st->xgwa.visual, st->xgwa.colormap,
                          colors, &ncolors, True, True, 0, False);
  }

  st->border = get_integer_resource ("borderSize", "BorderSize");

  {
    char *fontname = get_string_resource ("font", "Font");
    st->font = XLoadQueryFont (dpy, fontname);

    if (!st->font)
      {
	static const char *fonts[] = {
          "-*-courier-medium-r-*-*-*-1400-*-*-p-*-*-*",
          "-*-courier-medium-r-*-*-*-600-*-*-p-*-*-*",
          "-*-utopia-*-r-*-*-*-1400-*-*-p-*-*-*",
          "-*-utopia-*-r-*-*-*-600-*-*-p-*-*-*",
          "-*-utopia-*-r-*-*-*-240-*-*-p-*-*-*",
          "-*-helvetica-*-r-*-*-*-240-*-*-p-*-*-*",
          "-*-*-*-r-*-*-*-240-*-*-m-*-*-*",
          "fixed", 0 };
        i = 0;
        while (fonts[i])
          {
            st->font = XLoadQueryFont (dpy, fonts[i]);
            if (st->font) break;
            i++;
          }
        if (st->font)
          fprintf (stderr, "%s: couldn't load font \"%s\", using \"%s\"\n",
                   progname, fontname, fonts[i]);
        else
          {
            fprintf(stderr, "%s: couldn't load any font\n", progname);
            exit(-1);
          }
      }
  }

  gcv.line_width = st->border;

  gcv.background = get_pixel_resource("background", "Background",
                                      st->dpy, st->xgwa.colormap);
  gcv.foreground = get_pixel_resource("textColor", "Foreground",
                                      st->dpy, st->xgwa.colormap);
  gcv.font = st->font->fid;
  st->text_gc = XCreateGC (st->dpy, st->window,
                           GCForeground|GCBackground|GCFont, &gcv);

  gcv.foreground = get_pixel_resource("foreground", "Foreground",
                                      st->dpy, st->xgwa.colormap);
  st->draw_gc = XCreateGC (st->dpy, st->window,
                           GCForeground|GCBackground|GCLineWidth,
                           &gcv);
  gcv.foreground = gcv.background;
  st->erase_gc = XCreateGC (st->dpy, st->window,
                            GCForeground|GCBackground, &gcv);


  s = get_string_resource ("drawMode", "DrawMode");
  if (!s || !*s || !strcasecmp (s, "color"))
    st->draw_mode = DRAW_COLOR;
  else if (!strcasecmp (s, "mono"))
    st->draw_mode = DRAW_MONO;
  else
    {
      fprintf (stderr, "%s: drawMode must be 'mono' or 'color', not '%s'\n",
               progname, s);
      exit (1);
    }
  if (s) free (s);
  s = 0;


  st->filename = get_string_resource ("filename", "Filename");

  if (!st->filename ||
      !*st->filename ||
      !strcasecmp (st->filename, "(ram)") ||
      !strcasecmp (st->filename, "(mem)") ||
      !strcasecmp (st->filename, "(memory)"))
    st->seed_mode = SEED_RAM;
  else if (st->filename &&
           (!strcasecmp (st->filename, "(rand)") ||
            !strcasecmp (st->filename, "(random)")))
    st->seed_mode = SEED_RANDOM;
  else
    st->seed_mode = SEED_FILE;

  st->nscrollers = 3;
  st->scrollers = (scroller *) calloc (st->nscrollers, sizeof(scroller));

  for (i = 0; i < st->nscrollers; i++)
    {
      scroller *sc = &st->scrollers[i];
      int max_height = 4096;

      sc->which = i;
      sc->speed = i+1;

      sc->image = 0;
# ifdef HAVE_XSHM_EXTENSION
      st->shm_p = get_boolean_resource ("useSHM", "Boolean");
      if (st->shm_p)
        {
          sc->image = create_xshm_image (st->dpy, st->xgwa.visual,
                                         st->xgwa.depth,
                                         ZPixmap, 0, &st->shm_info,
                                         1, max_height);
          if (! sc->image)
            st->shm_p = False;
        }
# endif /* HAVE_XSHM_EXTENSION */

      if (!sc->image)
        sc->image = XCreateImage (st->dpy, st->xgwa.visual, st->xgwa.depth,
                                  ZPixmap, 0, 0, 1, max_height, 8, 0);

      if (sc->image && !sc->image->data)
        sc->image->data = (char *)
          malloc (sc->image->bytes_per_line * sc->image->height + 1);

      if (!sc->image || !sc->image->data)
        {
          fprintf (stderr, "%s: out of memory (allocating 1x%d image)\n",
                   progname, sc->image->height);
          exit (1);
        }
    }

  return st;
}


static void
reshape_memscroller (state *st)
{
  int i;

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  for (i = 0; i < st->nscrollers; i++)
    {
      scroller *sc = &st->scrollers[i];

      if (i == 0)
        {
          sc->rez = 6;  /* #### */

          sc->rect.width  = (((int) (st->xgwa.width * 0.8)
                              / sc->rez) * sc->rez);
          sc->rect.height = (((int) (st->xgwa.height * 0.3)
                              / sc->rez) * sc->rez);

          sc->rect.x = (st->xgwa.width  - sc->rect.width)  / 2;
          sc->rect.y = (st->xgwa.height - sc->rect.height) / 2;
        }
      else
        {
          scroller *sc0 = &st->scrollers[i-1];
          sc->rez = sc0->rez * 1.8;

          sc->rect.x      = sc0->rect.x;
          sc->rect.y      = (sc0->rect.y + sc0->rect.height + st->border
                             + (st->border + 2) * 7);
          sc->rect.width  = sc0->rect.width;
          sc->rect.height = (((int) (st->xgwa.height * 0.1)
                              / sc->rez) * sc->rez);
        }

      XDrawRectangle (st->dpy, st->window, st->draw_gc,
                      sc->rect.x - st->border*2,
                      sc->rect.y - st->border*2,
                      sc->rect.width  + st->border*4,
                      sc->rect.height + st->border*4);
    }
}



static void
open_file (state *st)
{
  if (st->in)
    {
      fclose (st->in);
      st->in = 0;
    }

  st->in = fopen (st->filename, "r");
  if (!st->in)
    {
      char buf[1024];
      sprintf (buf, "%s: %s", progname, st->filename);
      perror (buf);
      exit (1);
    }
}


static unsigned int
more_bits (state *st, scroller *sc)
{
  static unsigned char *lomem = 0;
  static unsigned char *himem = 0;
  unsigned char r, g, b;
  unsigned int v;

  /* Pack bytes as RGBR so that we don't have to worry about actually
     figuring out the color channel order.
   */
# undef PACK
# define PACK(r,g,b) ((r) | ((g) << 8) | ((b) << 16) | ((r) << 24))

  switch (st->seed_mode)
    {
    case SEED_RAM:
      if (himem == 0)
        {
          lomem = (unsigned char *) progname; /* not first malloc, but early */
          himem = (unsigned char *)           /* not last malloc, but late */
            st->scrollers[st->nscrollers-1].image->data;
        }

      if (sc->data < lomem)
        sc->data = lomem;

# ifdef HAVE_SBRK  /* re-get it each time through */
      himem = ((unsigned char *) sbrk(0)) - (2 * sizeof(void *));
# endif

      /* I don't understand what's going on there, but on MacOS X,
         we're getting insane values for lomem and himem.  Is there
         more than one heap? */
      if ((unsigned long) himem - (unsigned long) lomem > 0x0FFFFFFF)
        himem = lomem + 0xFFFF;

      if (lomem >= himem) abort();

    RETRY:
      if (sc->data >= himem)
        sc->data = lomem;

      switch (st->draw_mode)
        {
        case DRAW_COLOR:
          r = *sc->data++;
          g = *sc->data++;
          b = *sc->data++;
          break;
        case DRAW_MONO:
          r = 0;
          g = *sc->data++;
          b = 0;
          break;
        default:
          abort();
        }

      v = PACK(r,g,b);

      /* avoid having many seconds of blackness: truncate zeros at 24K.
       */
      if (v == 0)
        sc->count_zero++;
      else
        sc->count_zero = 0;
      if (sc->count_zero > 1024 * (st->draw_mode == DRAW_COLOR ? 24 : 8))
        goto RETRY;

      break;

    case SEED_RANDOM:
      v = random();
      switch (st->draw_mode)
        {
        case DRAW_COLOR:
          r = (v >> 16) & 0xFF;
          g = (v >>  8) & 0xFF;
          b = (v      ) & 0xFF;
          break;
        case DRAW_MONO:
          r = 0;
          g = v & 0xFF;
          b = 0;
          break;
        default:
          abort();
        }
      v = PACK(r,g,b);
      break;

    case SEED_FILE:
      {
        int i;

  /* this one returns only bytes from the file */
# define GETC(V) \
            do { \
              i = fgetc (st->in); \
            } while (i == EOF \
                     ? (open_file (st), 1) \
                     : 0); \
            V = i

  /* this one returns a null at EOF -- else we hang on zero-length files */
# undef GETC
# define GETC(V) \
            i = fgetc (st->in); \
            if (i == EOF) { i = 0; open_file (st); } \
            V = i

        if (!st->in)
          open_file (st);

        switch (st->draw_mode)
          {
          case DRAW_COLOR:
            GETC(r);
            GETC(g);
            GETC(b);
            break;
          case DRAW_MONO:
            r = 0;
            GETC(g);
            b = 0;
            break;
          default:
            abort();
          }
# undef GETC
        v = PACK(r,g,b);
      }
      break;

    default:
      abort();
    }

# undef PACK
  return v;
}


static void
draw_string (state *st, unsigned int n)
{
  char buf[40];
  int direction, ascent, descent;
  XCharStruct overall;
  int x, y, w, h;
  int bot = st->scrollers[0].rect.y;
  const char *fmt = "%08X";

  sprintf (buf, fmt, 0);
  XTextExtents (st->font, buf, strlen(buf), 
                &direction, &ascent, &descent, &overall);
  sprintf (buf, "%08X", n);

  w = overall.width;
  h = st->font->ascent + st->font->descent + 1;
  x = (st->xgwa.width - w) / 2;
  y = (bot - h) / 2;

  if (y + h + 10 > bot) return;

  XFillRectangle (st->dpy, st->window, st->erase_gc,
                  x-w, y, w*3, h);
  XDrawString (st->dpy, st->window, st->text_gc,
               x, y + st->font->ascent, buf, strlen(buf));
}


static void
draw_memscroller (state *st)
{
  int i;

  draw_string (st, *((unsigned int *) st->scrollers[0].image->data));

  for (i = 0; i < st->nscrollers; i++)
    {
      scroller *sc = &st->scrollers[i];
      int j;

      XCopyArea (st->dpy, st->window, st->window, st->draw_gc,
                 sc->rect.x + sc->speed, sc->rect.y,
                 sc->rect.width - sc->speed, sc->rect.height,
                 sc->rect.x, sc->rect.y);

      if (sc->scroll_tick == 0)
        {
          int top = ((sc->image->bytes_per_line * sc->rect.height) /
                     (4 * sc->rez));
          unsigned int *out = (unsigned int *) sc->image->data;
          for (j = 0; j < top; j++)
            {
              unsigned int v = more_bits(st, sc);
              int k;
              for (k = 0; k < sc->rez; k++)
                *out++ = v;
            }
        }

      sc->scroll_tick++;
      if (sc->scroll_tick * sc->speed >= sc->rez)
        sc->scroll_tick = 0;

      for (j = 0; j < sc->speed; j++)
        {
# ifdef HAVE_XSHM_EXTENSION
          if (st->shm_p)
            XShmPutImage (st->dpy, st->window, st->draw_gc, sc->image,
                          0, 0,
                          sc->rect.x + sc->rect.width - sc->image->width - j,
                          sc->rect.y,
                          sc->rect.width, sc->rect.height,
                          False);
          else
# endif /* HAVE_XSHM_EXTENSION */
            XPutImage (st->dpy, st->window, st->draw_gc, sc->image,
                       0, 0,
                       sc->rect.x + sc->rect.width - sc->image->width - j,
                       sc->rect.y,
                       sc->rect.width, sc->rect.height);
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
        {
          XClearWindow (st->dpy, st->window);
          reshape_memscroller (st);
        }

      screenhack_handle_event (st->dpy, &event);
    }
}



char *progclass = "MemScroller";

char *defaults [] = {
  ".background:		   black",
  "*drawMode:		   color",
  "*filename:		   (RAM)",
  ".textColor:		   #00FF00",
  ".foreground:		   #00FF00",
  "*borderSize:		   2",
  ".font:		   -*-courier-medium-r-*-*-*-1400-*-*-m-*-*-*",
  "*delay:		   10000",
  "*offset:		   0",
  0
};

XrmOptionDescRec options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-font",		".font",		XrmoptionSepArg, 0 },
  { "-filename",	".filename",		XrmoptionSepArg, 0 },
  { "-color",		".drawMode",		XrmoptionNoArg, "color"    },
  { "-mono",		".drawMode",		XrmoptionNoArg, "mono"     },
  { "-ram",		".filename",		XrmoptionNoArg, "(RAM)"    },
  { "-random",		".filename",		XrmoptionNoArg, "(RANDOM)" },
  { 0, 0, 0, 0 }
};


void
screenhack (Display *dpy, Window window)
{
  state *st = init_memscroller (dpy, window);
  int delay = get_integer_resource ("delay", "Integer");
  reshape_memscroller (st);
  while (1)
    {
      draw_memscroller (st);
      XSync (dpy, False);
      handle_events (st);
      if (delay) usleep (delay);
    }
}
