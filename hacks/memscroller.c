/* xscreensaver, Copyright (c) 2002, 2004, 2006 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_XSHM_EXTENSION
#include "xshm.h"
#endif

#undef countof
#define countof(x) (sizeof(x)/sizeof(*(x)))

typedef struct {
  int which;
  XRectangle rect;
  XImage *image;
  int rez;
  int speed;
  int scroll_tick;
  unsigned int value;
  unsigned char *data;
  int count_zero;
} scroller;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  GC draw_gc, erase_gc, text_gc;
  XFontStruct *fonts[6];
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

  int delay;

} state;


static void reshape_memscroller (state *st);


static void *
memscroller_init (Display *dpy, Window window)
{
  int i;
  XGCValues gcv;
  state *st = (state *) calloc (1, sizeof (*st));
  char *s;
  st->dpy = dpy;
  st->window = window;
  st->delay = get_integer_resource (dpy, "delay", "Integer");

  XGetWindowAttributes (st->dpy, st->window, &st->xgwa);

  /* Fill up the colormap with random colors.
     We don't actually use these explicitly, but in 8-bit mode,
     they will be used implicitly by the random image bits. */
  {
    int ncolors = 255;
    XColor colors[256];
    make_random_colormap (st->dpy, st->xgwa.visual, st->xgwa.colormap,
                          colors, &ncolors, True, True, 0, False);
  }

  st->border = get_integer_resource (dpy, "borderSize", "BorderSize");

  {
    int i;
    int nfonts = countof (st->fonts);
    for (i = nfonts-1; i >= 0; i--)
      {
        char *fontname;
        char res[20];
        sprintf (res, "font%d", i+1);
        fontname = get_string_resource (dpy, res, "Font");
        st->fonts[i] = XLoadQueryFont (dpy, fontname);
        if (!st->fonts[i] && i < nfonts-1)
          {
            fprintf (stderr, "%s: unable to load font: \"%s\"\n",
                     progname, fontname);
            st->fonts[i] = st->fonts[i+1];
          }
      }
    if (!st->fonts[0])
      {
        fprintf (stderr, "%s: unable to load any fonts!", progname);
        exit (1);
      }
  }

  gcv.line_width = st->border;

  gcv.background = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                      "background", "Background");
  gcv.foreground = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                      "textColor", "Foreground");
  st->text_gc = XCreateGC (st->dpy, st->window,
                           GCForeground|GCBackground, &gcv);

  gcv.foreground = get_pixel_resource(st->dpy, st->xgwa.colormap,
                                      "foreground", "Foreground");
  st->draw_gc = XCreateGC (st->dpy, st->window,
                           GCForeground|GCBackground|GCLineWidth,
                           &gcv);
  gcv.foreground = gcv.background;
  st->erase_gc = XCreateGC (st->dpy, st->window,
                            GCForeground|GCBackground, &gcv);


  s = get_string_resource (dpy, "drawMode", "DrawMode");
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


  st->filename = get_string_resource (dpy, "filename", "Filename");

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
      st->shm_p = get_boolean_resource (dpy, "useSHM", "Boolean");
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

  reshape_memscroller (st);
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

  /* vv: Each incoming byte rolls through all 4 bytes of this (it is sc->value)
         This is the number displayed at the top.
     pv: the pixel color value.  incoming bytes land in R,G,B, or maybe just G.
   */
  unsigned int vv, pv;

  unsigned int rmsk = st->scrollers[0].image->red_mask;
  unsigned int gmsk = st->scrollers[0].image->green_mask;
  unsigned int bmsk = st->scrollers[0].image->blue_mask;
  unsigned int amsk = ~(rmsk | gmsk | bmsk);

  vv = sc->value;

  /* Pack RGB into a pixel according to the XImage component masks;
     set the remaining bits to 1 for the benefit of HAVE_COCOA alpha.
   */
# undef PACK
# define PACK() ((((r << 24) | (r << 16) | (r << 8) | r) & rmsk) | \
                 (((g << 24) | (g << 16) | (g << 8) | g) & gmsk) | \
                 (((b << 24) | (b << 16) | (b << 8) | b) & bmsk) | \
                 amsk)

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
      /* "The brk and sbrk functions are historical curiosities left over
         from earlier days before the advent of virtual memory management."
            -- sbrk(2) man page on MacOS
       */
      himem = ((unsigned char *) sbrk(0)) - (2 * sizeof(void *));
# endif

      if (!lomem || !himem) 
        {
          /* bad craziness! give up! */
          st->seed_mode = SEED_RANDOM;
          return 0;
        }

      /* I don't understand what's going on there, but on MacOS X, we're
         getting insane values for lomem and himem (both Xlib and HAVE_COCOA).
         Does malloc() draw from more than one heap? */
      if ((unsigned long) himem - (unsigned long) lomem > 0x0FFFFFFF) {
# if 0
        fprintf (stderr, "%s: wonky: 0x%08x - 0x%08x = 0x%08x\n", progname,
                 (unsigned int) himem,  (unsigned int) lomem,
                 (unsigned int) himem - (unsigned int) lomem);
# endif
        himem = lomem + 0xFFFF;
      }

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
          vv = (vv << 24) | (r << 16) | (g << 8) | b;
          break;
        case DRAW_MONO:
          r = 0;
          g = *sc->data++;
          b = 0;
          vv = (vv << 8) | g;
          break;
        default:
          abort();
        }

      pv = PACK();

      /* avoid having many seconds of blackness: truncate zeros at 24K.
       */
      if (vv == 0)
        sc->count_zero++;
      else
        sc->count_zero = 0;
      if (sc->count_zero > 1024 * (st->draw_mode == DRAW_COLOR ? 24 : 8))
        goto RETRY;

      break;

    case SEED_RANDOM:
      vv = random();
      switch (st->draw_mode)
        {
        case DRAW_COLOR:
          r = (vv >> 16) & 0xFF;
          g = (vv >>  8) & 0xFF;
          b = (vv      ) & 0xFF;
          break;
        case DRAW_MONO:
          r = 0;
          g = vv & 0xFF;
          b = 0;
          break;
        default:
          abort();
        }
      pv = PACK();
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
            vv = (vv << 24) | (r << 16) | (g << 8) | b;
            break;
          case DRAW_MONO:
            r = 0;
            GETC(g);
            b = 0;
            vv = (vv << 8) | g;
            break;
          default:
            abort();
          }
# undef GETC
        pv = PACK();
      }
      break;

    default:
      abort();
    }

# undef PACK

  sc->value = vv;
  return pv;
}


static void
draw_string (state *st)
{
  char buf[40];
  int direction, ascent, descent;
  int bot = st->scrollers[0].rect.y;
  const char *fmt = "%08X";
  int i;

  /* Draw the first font that fits.
   */
  for (i = 0; i < countof (st->fonts); i++)
    {
      XCharStruct overall;
      int x, y, w, h;

      if (! st->fonts[i]) continue;

      sprintf (buf, fmt, 0);
      XTextExtents (st->fonts[i], buf, strlen(buf), 
                    &direction, &ascent, &descent, &overall);
      sprintf (buf, "%08X", st->scrollers[0].value);

      w = overall.width;
      h = ascent + descent + 1;
      x = (st->xgwa.width - w) / 2;
      y = (bot - h) / 2;

      if (y + h + 10 <= bot && x > -10)
        {
          XSetFont (st->dpy, st->text_gc, st->fonts[i]->fid);
          XFillRectangle (st->dpy, st->window, st->erase_gc,
                          x-w, y, w*3, h);
          XDrawString (st->dpy, st->window, st->text_gc,
                       x, y + ascent, buf, strlen(buf));
          break;
        }
    }
}


static unsigned long
memscroller_draw (Display *dpy, Window window, void *closure)
{
  state *st = (state *) closure;
  int i;
  draw_string (st);

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

  return st->delay;
}


static void
memscroller_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  state *st = (state *) closure;
  XClearWindow (st->dpy, st->window);
  reshape_memscroller (st);
}

static Bool
memscroller_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}


static void
memscroller_free (Display *dpy, Window window, void *closure)
{
}


static const char *memscroller_defaults [] = {
  ".background:		   black",
  "*drawMode:		   color",
  "*fpsSolid:		   true",
  "*fpsTop:		   true",
  "*filename:		   (RAM)",
  ".textColor:		   #00FF00",
  ".foreground:		   #00FF00",
  "*borderSize:		   2",
#ifdef HAVE_COCOA
  ".font1:		   OCR A Std 192",
  ".font2:		   OCR A Std 144",
  ".font3:		   OCR A Std 128",
  ".font4:		   OCR A Std 96",
  ".font5:		   OCR A Std 48",
  ".font6:		   OCR A Std 24",
#else
  ".font1:		   -*-courier-bold-r-*-*-*-1440-*-*-m-*-*-*",
  ".font2:		   -*-courier-bold-r-*-*-*-960-*-*-m-*-*-*",
  ".font3:		   -*-courier-bold-r-*-*-*-480-*-*-m-*-*-*",
  ".font4:		   -*-courier-bold-r-*-*-*-320-*-*-m-*-*-*",
  ".font5:		   -*-courier-bold-r-*-*-*-180-*-*-m-*-*-*",
  ".font6:		   fixed",
#endif
  "*delay:		   10000",
  "*offset:		   0",
  0
};

static XrmOptionDescRec memscroller_options [] = {
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-font",		".font",		XrmoptionSepArg, 0 },
  { "-filename",	".filename",		XrmoptionSepArg, 0 },
  { "-color",		".drawMode",		XrmoptionNoArg, "color"    },
  { "-mono",		".drawMode",		XrmoptionNoArg, "mono"     },
  { "-ram",		".filename",		XrmoptionNoArg, "(RAM)"    },
  { "-random",		".filename",		XrmoptionNoArg, "(RANDOM)" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("MemScroller", memscroller)
