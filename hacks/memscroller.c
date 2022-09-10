/* xscreensaver, Copyright © 2002-2021 Jamie Zawinski <jwz@jwz.org>
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
#include "xshm.h"
#include "xft.h"
#include <stdio.h>

#ifndef HAVE_MOBILE
# define READ_FILES
#endif

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
  XftColor xft_fg;
  XftDraw *xftdraw;
  XftFont *fonts[6];
  int border;

  enum { SEED_RAM, SEED_RANDOM, SEED_FILE } seed_mode;
  enum { DRAW_COLOR, DRAW_MONO } draw_mode;

  char *filename;
  FILE *in;

  int nscrollers;
  scroller *scrollers;

  XShmSegmentInfo shm_info;

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
    make_random_colormap (st->xgwa.screen, st->xgwa.visual, st->xgwa.colormap,
                          colors, &ncolors, True, True, 0, False);
  }

  st->border = get_integer_resource (dpy, "borderSize", "BorderSize");
  if (st->xgwa.width > 2560) st->border *= 2;  /* Retina displays */

  {
    int i;
    int nfonts = countof (st->fonts);
    for (i = 0; i < nfonts; i++)
      {
        char *fontname;
        char res[20];
        sprintf (res, "font%d", i+1);
        fontname = get_string_resource (dpy, res, "Font");
        if (fontname && *fontname)
          st->fonts[i] =
            load_xft_font_retry (dpy, screen_number (st->xgwa.screen),
                                 fontname);
        if (fontname) free (fontname);
      }
    if (!st->fonts[0]) abort();
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
  s = get_string_resource (st->dpy, "foreground", "Foreground");
  if (!s) s = strdup ("white");
  XftColorAllocName (st->dpy, st->xgwa.visual, st->xgwa.colormap, s,
                     &st->xft_fg);
  free (s);
  st->xftdraw = XftDrawCreate (dpy, window, st->xgwa.visual,
                               st->xgwa.colormap);

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


# ifdef READ_FILES
  st->filename = get_string_resource (dpy, "filename", "Filename");
# endif

  if (!st->filename ||
      !*st->filename ||
      !strcasecmp (st->filename, "(ram)") ||
      !strcasecmp (st->filename, "(mem)") ||
      !strcasecmp (st->filename, "(memory)"))
    st->seed_mode = SEED_RAM;
# ifdef READ_FILES
  else if (st->filename &&
           (!strcasecmp (st->filename, "(rand)") ||
            !strcasecmp (st->filename, "(random)")))
    st->seed_mode = SEED_RANDOM;
  else
    st->seed_mode = SEED_FILE;
# else
  st->seed_mode = SEED_RANDOM;
# endif

  st->nscrollers = 3;
  st->scrollers = (scroller *) calloc (st->nscrollers, sizeof(scroller));

  for (i = 0; i < st->nscrollers; i++)
    {
      scroller *sc = &st->scrollers[i];
      int max_height = 4096;

      sc->which = i;
      sc->speed = i+1;

      if (st->xgwa.width > 2560) sc->speed *= 2.5;  /* Retina displays */

      sc->image = create_xshm_image (st->dpy, st->xgwa.visual,
                                     st->xgwa.depth,
                                     ZPixmap, &st->shm_info,
                                     1, max_height);

      if (!sc->image)
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

          if (st->xgwa.width > 2560) sc->rez *= 2.5;  /* Retina displays */

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



# ifdef READ_FILES
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
#endif


/* "The brk and sbrk functions are historical curiosities left over
   from earlier days before the advent of virtual memory management."
      -- sbrk(2) man page on BSD systems, as of 1995 or so.
 */
#ifdef HAVE_SBRK
# if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) /* gcc >= 4.2 */
   /* Don't print "warning: 'sbrk' is deprecated". */
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# endif
#endif


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
     set the remaining bits to 1 for the benefit of HAVE_JWXYZ alpha.
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
      himem = ((unsigned char *) sbrk(0)) - (2 * sizeof(void *));
# endif

      if (!lomem || !himem) 
        {
          /* bad craziness! give up! */
          st->seed_mode = SEED_RANDOM;
          return 0;
        }

      /* I don't understand what's going on there, but on MacOS X, we're
         getting insane values for lomem and himem (both Xlib and HAVE_JWXYZ).
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

# ifdef READ_FILES
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
# endif /* READ_FILES */

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
  int bot = st->scrollers[0].rect.y;
  const char *fmt = "%08X";
  int i;

  /* Draw the first font that fits.
   */
  for (i = 0; i < countof (st->fonts); i++)
    {
      XGlyphInfo overall;
      int x, y, w, h;

      if (! st->fonts[i]) continue;

      sprintf (buf, fmt, 0);
      XftTextExtentsUtf8 (st->dpy, st->fonts[i], (FcChar8 *) buf, 
                          strlen(buf), &overall);
      sprintf (buf, "%08X", st->scrollers[0].value);

      w = overall.width;
      h = st->fonts[i]->ascent + st->fonts[i]->descent + 1;
      x = (st->xgwa.width - w) / 2;
      y = (bot - h) / 2;

      if (y + h + 10 <= bot && x > -10)
        {
          XFillRectangle (st->dpy, st->window, st->erase_gc,
                          x-w-1, y-1, w*3+2, h+2);
          XftDrawStringUtf8 (st->xftdraw, &st->xft_fg, st->fonts[i],
                             x, y + st->fonts[i]->ascent,
                             (FcChar8 *) buf, strlen(buf));
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
          put_xshm_image (st->dpy, st->window, st->draw_gc, sc->image,
                          0, 0,
                          sc->rect.x + sc->rect.width - sc->image->width - j,
                          sc->rect.y,
                          sc->rect.width, sc->rect.height,
                          &st->shm_info);
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
  state *st = (state *) closure;
  int i;
  for (i = 0; i < st->nscrollers; i++)
    destroy_xshm_image (dpy, st->scrollers[i].image, &st->shm_info);
  free (st->scrollers);
  for (i = 0; i < countof (st->fonts); i++)
    if (st->fonts[i]) XftFontClose (st->dpy, st->fonts[i]);
  if (st->filename) free (st->filename);
  XFreeGC (dpy, st->draw_gc);
  XFreeGC (dpy, st->erase_gc);
  XFreeGC (dpy, st->text_gc);
  XftDrawDestroy (st->xftdraw);
  free (st);
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

  ".font1:	OCR A Std 192, Lucida Console 192, Monaco 192, Courier 192",
  ".font2:	OCR A Std 144, Lucida Console 144, Monaco 144, Courier 144",
  ".font3:	OCR A Std 128, Lucida Console 128, Monaco 128, Courier 128",
  ".font4:	OCR A Std 96,  Lucida Console 96,  Monaco 96,  Courier 96",
  ".font5:	OCR A Std 48,  Lucida Console 48,  Monaco 48,  Courier 48",
  ".font6:	OCR A Std 24,  Lucida Console 24,  Monaco 24,  Courier 24",

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
