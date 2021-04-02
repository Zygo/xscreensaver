/* xscreensaver, Copyright © 1999-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Phosphor -- simulate a glass tty with long-sustain phosphor.
 * Written by Jamie Zawinski <jwz@jwz.org>
 * Pty and vt100 emulation by Fredrik Tolf <fredrik@dolda2000.com>
 */

#include "screenhack.h"
#include "textclient.h"
#include "ximage-loader.h"
#include "utf8wc.h"

#ifndef HAVE_JWXYZ
# include <X11/Intrinsic.h>
#endif


#define FUZZY_BORDER

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define BLANK  0
#define FLARE  1
#define NORMAL 2
#define FADE   3
#define STATE_MAX FADE

#define CURSOR_INDEX 128

#define NPAR 16

#define BUILTIN_FONT

#ifdef BUILTIN_FONT
# include "images/gen/6x10font_png.h"
#endif /* BUILTIN_FONT */

typedef struct {
  unsigned char name;
  int width, height;
  Pixmap pixmap;
#ifdef FUZZY_BORDER
  Pixmap pixmap2;
#endif /* FUZZY_BORDER */
  Bool blank_p;
} p_char;

typedef struct {
  p_char *p_char;
  int state;
  Bool changed;
} p_cell;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  XftFont *font;
  XftDraw *xftdraw;
  const char *program;
  int grid_width, grid_height;
  int char_width, char_height;
  int xmargin, ymargin;
  int saved_x, saved_y;
  int scale;
  int ticks;
  int mode;

  int escstate;
  int csiparam[NPAR];
  int curparam;
  int unicruds; unsigned char unicrud[7];

  p_char **chars;
  p_cell *cells;
  XGCValues gcv;
  GC gc0;
  GC gc1;
#ifdef FUZZY_BORDER
  GC gc2;
#endif /* FUZZY_BORDER */
  GC *gcs;
  XImage *font_bits;

  int cursor_x, cursor_y;
  XtIntervalId cursor_timer;
  Time cursor_blink;
  int delay;
  Bool pty_p;

  text_data *tc;

  char last_c;
  int bk;

} p_state;


static void capture_font_bits (p_state *state);
static p_char *make_character (p_state *state, int c);
static void char_to_pixmap (p_state *state, p_char *pc, int c);


/* About font metrics:

   "lbearing" is the distance from the leftmost pixel of a character to
   the logical origin of that character.  That is, it is the number of
   pixels of the character which are to the left of its logical origin.

   "rbearing" is the distance from the logical origin of a character to
   the rightmost pixel of a character.  That is, it is the number of
   pixels of the character to the right of its logical origin.

   "descent" is the distance from the bottommost pixel of a character to
   the logical baseline.  That is, it is the number of pixels of the
   character which are below the baseline.

   "ascent" is the distance from the logical baseline to the topmost pixel.
   That is, it is the number of pixels of the character above the baseline.

   Therefore, the bounding box of the "ink" of a character is
   lbearing + rbearing by ascent + descent;

   "width" is the distance from the logical origin of this character to
   the position where the logical orgin of the next character should be
   placed.

   For our purposes, we're only interested in the part of the character
   lying inside the "width" box.  If the characters have ink outside of
   that box (the "charcell" box) then we're going to lose it.  Alas.
 */


static void clear (p_state *);
static void set_cursor (p_state *, Bool on);

static unsigned short scale_color_channel (unsigned short ch1, unsigned short ch2)
{
  return (ch1 * 100 + ch2 * 156) >> 8;
}

#define FONT6x10_WIDTH (256*7)
#define FONT6x10_HEIGHT 10

static void *
phosphor_init (Display *dpy, Window window)
{
  int i;
  unsigned long flags;
  p_state *state = (p_state *) calloc (sizeof(*state), 1);
  char *fontname = get_string_resource (dpy, "font", "Font");
  XftFont *font;

  state->dpy = dpy;
  state->window = window;

  XGetWindowAttributes (dpy, window, &state->xgwa);
/*  XSelectInput (dpy, window, state->xgwa.your_event_mask | ExposureMask);*/

  state->delay = get_integer_resource (dpy, "delay", "Integer");
  state->pty_p = get_boolean_resource (dpy, "usePty", "UsePty");

  if (!strcasecmp (fontname, "builtin") ||
      !strcasecmp (fontname, "(builtin)"))
    {
#ifndef BUILTIN_FONT
      fprintf (stderr, "%s: no builtin font\n", progname);
      state->font = load_xft_font_retry (dpy,
                                         screen_number (state->xgwa.screen),
                                         "fixed");
#endif /* !BUILTIN_FONT */
    }
  else
    {
      state->font = load_xft_font_retry (dpy,
                                         screen_number (state->xgwa.screen),
                                         fontname);
      if (!state->font) abort();
    }

  if (fontname) free (fontname);

  font = state->font;
  state->xftdraw = XftDrawCreate (dpy, window, state->xgwa.visual,
                               state->xgwa.colormap);

  /* Xft uses 'scale' */
  state->scale = get_integer_resource (dpy, "phosphorScale", "Integer");
  state->ticks = STATE_MAX + get_integer_resource (dpy, "ticks", "Integer");
  state->escstate = 0;

  if (state->xgwa.width > 2560) state->scale *= 2;  /* Retina displays */

  state->cursor_blink = get_integer_resource (dpy, "cursor", "Time");

# ifdef BUILTIN_FONT
  if (! font)
    {
      state->char_width  = (FONT6x10_WIDTH / 256) - 1;
      state->char_height = FONT6x10_HEIGHT;
    }
  else
# endif /* BUILTIN_FONT */
    {
      XGlyphInfo overall;
      XftTextExtentsUtf8 (dpy, state->font, (FcChar8 *) "N", 1, &overall);
      state->char_width  = overall.xOff;
      state->char_height = font->ascent + font->descent;
    }

# ifdef HAVE_IPHONE
  /* Stupid iPhone X bezel.
     #### This is the worst of all possible ways to do this!  But how else?
   */
  if (state->xgwa.width == 2436 || state->xgwa.height == 2436) {
    state->xmargin = 96;
    state->ymargin = state->xmargin;
  }
# endif

  state->grid_width = ((state->xgwa.width - state->xmargin * 2) /
                       (state->char_width * state->scale));
  state->grid_height = ((state->xgwa.height - state->ymargin * 2) /
                        (state->char_height * state->scale));
  state->cells = (p_cell *) calloc (sizeof(p_cell),
                                    state->grid_width * state->grid_height);
  state->chars = (p_char **) calloc (sizeof(p_char *), 256);

  state->gcs = (GC *) calloc (sizeof(GC), state->ticks + 1);

  {
    int ncolors = MAX (1, state->ticks - 3);
    XColor *colors = (XColor *) calloc (ncolors, sizeof(XColor));
    int h1, h2;
    double s1, s2, v1, v2;

    unsigned long fg = get_pixel_resource (state->dpy, state->xgwa.colormap,
                                           "foreground", "Foreground");
    unsigned long bg = get_pixel_resource (state->dpy, state->xgwa.colormap,
                                           "background", "Background");
    unsigned long flare = fg;

    XColor fg_color, bg_color;

    fg_color.pixel = fg;
    XQueryColor (state->dpy, state->xgwa.colormap, &fg_color);

    bg_color.pixel = bg;
    XQueryColor (state->dpy, state->xgwa.colormap, &bg_color);

    /* Now allocate a ramp of colors from the main color to the background. */
    rgb_to_hsv (scale_color_channel(fg_color.red, bg_color.red),
                scale_color_channel(fg_color.green, bg_color.green),
                scale_color_channel(fg_color.blue, bg_color.blue),
                &h1, &s1, &v1);
    rgb_to_hsv (bg_color.red, bg_color.green, bg_color.blue, &h2, &s2, &v2);

    /* Avoid rainbow effects when fading to black/grey/white. */
    if (s2 < 0.003)
      h2 = h1;
    if (s1 < 0.003)
      h1 = h2;

    make_color_ramp (state->xgwa.screen, state->xgwa.visual,
                     state->xgwa.colormap,
                     h1, s1, v1,
                     h2, s2, v2,
                     colors, &ncolors,
                     False, True, False);

    /* Adjust to the number of colors we actually got. */
    state->ticks = ncolors + STATE_MAX;

    /* If the foreground is brighter than the background, the flare is white.
     * Otherwise, the flare is left at the foreground color (i.e. no flare). */
    rgb_to_hsv (fg_color.red, fg_color.green, fg_color.blue, &h1, &s1, &v1);
    if (v2 <= v1)
      {
        XColor white;
        /* WhitePixel is only for the default visual, which can be overridden
         * on the command line. */
        white.red = 0xffff;
        white.green = 0xffff;
        white.blue = 0xffff;
        if (XAllocColor(state->dpy, state->xgwa.colormap, &white))
          flare = white.pixel;
      }

    /* Now, GCs all around.
     */
    state->gcv.cap_style = CapRound;
#ifdef FUZZY_BORDER
    state->gcv.line_width = (int) (((long) state->scale) * 1.3);
    if (state->gcv.line_width == state->scale)
      state->gcv.line_width++;
#else /* !FUZZY_BORDER */
    state->gcv.line_width = (int) (((long) state->scale) * 0.9);
    if (state->gcv.line_width >= state->scale)
      state->gcv.line_width = state->scale - 1;
    if (state->gcv.line_width < 1)
      state->gcv.line_width = 1;
#endif /* !FUZZY_BORDER */

    flags = (GCForeground | GCBackground | GCCapStyle | GCLineWidth);

    state->gcv.background = bg;
    state->gcv.foreground = bg;
    state->gcs[BLANK] = XCreateGC (state->dpy, state->window, flags,
                                   &state->gcv);

    state->gcv.foreground = flare;
    state->gcs[FLARE] = XCreateGC (state->dpy, state->window, flags,
                                   &state->gcv);

    state->gcv.foreground = fg;
    state->gcs[NORMAL] = XCreateGC (state->dpy, state->window, flags,
                                    &state->gcv);

    for (i = 0; i < ncolors; i++)
      {
        state->gcv.foreground = colors[i].pixel;
        state->gcs[STATE_MAX + i] = XCreateGC (state->dpy, state->window,
                                               flags, &state->gcv);
      }

    free (colors);
  }

  capture_font_bits (state);

  set_cursor (state, True);

/*  clear (state);*/

  state->tc = textclient_open (dpy);
  textclient_reshape (state->tc,
                      state->xgwa.width  - state->xmargin * 2,
                      state->xgwa.height - state->ymargin * 2,
                      state->grid_width  - 1,
                      state->grid_height - 1,
                      0);

  return state;
}


/* Re-query the window size and update the internal character grid if changed.
 */
static Bool
resize_grid (p_state *state)
{
  int ow = state->grid_width;
  int oh = state->grid_height;
  p_cell *ocells = state->cells;
  int x, y;

  XGetWindowAttributes (state->dpy, state->window, &state->xgwa);

  /* Would like to ensure here that
     state->char_height * state->scale <= state->xgwa.height
     but changing scale requires regenerating the bitmaps. */

  state->grid_width  = ((state->xgwa.width - state->xmargin * 2) /
                        (state->char_width  * state->scale));
  state->grid_height = ((state->xgwa.height - state->ymargin * 2) /
                        (state->char_height * state->scale));

  if (state->grid_width  < 2) state->grid_width  = 2;
  if (state->grid_height < 2) state->grid_height = 2;

  if (ow == state->grid_width &&
      oh == state->grid_height)
    return False;

  state->cells = (p_cell *) calloc (sizeof(p_cell),
                                    state->grid_width * state->grid_height);

  for (y = 0; y < state->grid_height; y++)
    {
      for (x = 0; x < state->grid_width; x++)
        {
          p_cell *ncell = &state->cells [state->grid_width * y + x];
          if (x < ow && y < oh)
            *ncell = ocells [ow * y + x];
          ncell->changed = True;
        }
    }

  if (state->cursor_x >= state->grid_width)
    state->cursor_x = state->grid_width-1;
  if (state->cursor_y >= state->grid_height)
    state->cursor_y = state->grid_height-1;

  free (ocells);
  return True;
}


static void
capture_font_bits (p_state *state)
{
  XftFont *font = state->font;
  int safe_width, height;
  unsigned char string[257];
  int i;
  Pixmap p;

# ifdef BUILTIN_FONT
  Pixmap p2 = 0;

  if (!font)
    {
      int pix_w, pix_h;
      XWindowAttributes xgwa;
      Pixmap m = 0;
      Pixmap p = image_data_to_pixmap (state->dpy, state->window,
                                       _6x10font_png, sizeof(_6x10font_png),
                                       &pix_w, &pix_h, &m);
      XImage *im = XGetImage (state->dpy, p, 0, 0, pix_w, pix_h, ~0L, ZPixmap);
      XImage *mm = XGetImage (state->dpy, m, 0, 0, pix_w, pix_h, 1, XYPixmap);
      XImage *im2;
      int x, y;
      XGCValues gcv;
      GC gc;
      unsigned long black =
        BlackPixelOfScreen (DefaultScreenOfDisplay (state->dpy));

      safe_width = state->char_width + 1;
      height = state->char_height;

      XFreePixmap (state->dpy, p);
      XFreePixmap (state->dpy, m);
      if (pix_w != 256*7) abort();
      if (pix_h != 10) abort();
      if (pix_w != FONT6x10_WIDTH) abort();
      if (pix_h != FONT6x10_HEIGHT) abort();

      XGetWindowAttributes (state->dpy, state->window, &xgwa);
      im2 = XCreateImage (state->dpy, xgwa.visual, 1, XYBitmap, 0, 0,
                          pix_w, pix_h, 8, 0);
      im2->data = malloc (im2->bytes_per_line * im2->height);

      /* Convert deep image to 1 bit */
      for (y = 0; y < pix_h; y++)
        for (x = 0; x < pix_w; x++)
          XPutPixel (im2, x, y,
                     (XGetPixel (mm, x, y)
                      ? (XGetPixel (im, x, y) == black)
                      : 0));

      XDestroyImage (im);
      XDestroyImage (mm);
      im = 0;

      p2 = XCreatePixmap (state->dpy, state->window, 
                          im2->width, im2->height, im2->depth);
      gcv.foreground = 1;
      gcv.background = 0;
      gc = XCreateGC (state->dpy, p2, GCForeground|GCBackground, &gcv);
      XPutImage (state->dpy, p2, gc, im2, 0, 0, 0, 0, im2->width, im2->height);
      XFreeGC (state->dpy, gc);
      XDestroyImage (im2);
    }
  else
# endif /* BUILTIN_FONT */
    {
      XGlyphInfo overall;
      XftTextExtentsUtf8 (state->dpy, state->font,
                          (FcChar8 *) "N", 1, &overall);
      /* #### maybe safe_width should take lbearing into account */
      safe_width  = overall.xOff;
      state->char_height = state->font->ascent + state->font->descent;
      height = state->char_height;
    }

  p = XCreatePixmap (state->dpy, state->window,
                     (safe_width * 256), height, 1);

  for (i = 0; i < 256; i++)
    string[i] = (unsigned char) i;
  string[256] = 0;

  state->gcv.foreground = 0;
  state->gcv.background = 0;
  state->gc0 = XCreateGC (state->dpy, p,
                          (GCForeground | GCBackground),
                          &state->gcv);

  state->gcv.foreground = 1;
  state->gc1 = XCreateGC (state->dpy, p,
                          (GCForeground | GCBackground |
                           GCCapStyle | GCLineWidth),
                          &state->gcv);

#ifdef HAVE_JWXYZ
  jwxyz_XSetAntiAliasing (state->dpy, state->gc0, False);
  jwxyz_XSetAntiAliasing (state->dpy, state->gc1, False);
#endif

#ifdef FUZZY_BORDER
  {
    state->gcv.line_width = (int) (((long) state->scale) * 0.8);
    if (state->gcv.line_width >= state->scale)
      state->gcv.line_width = state->scale - 1;
    if (state->gcv.line_width < 1)
      state->gcv.line_width = 1;
    state->gc2 = XCreateGC (state->dpy, p,
                            (GCForeground | GCBackground |
                             GCCapStyle | GCLineWidth),
                            &state->gcv);
  }
#endif /* FUZZY_BORDER */

  XFillRectangle (state->dpy, p, state->gc0, 0, 0, (safe_width * 256), height);

# ifdef BUILTIN_FONT
  if (p2)
    {
      XCopyPlane (state->dpy, p2, p, state->gc1,
                  0, 0, FONT6x10_WIDTH, FONT6x10_HEIGHT, 
                  0, 0, 1);
      XFreePixmap (state->dpy, p2);
    }
  else
# endif /* BUILTIN_FONT */
    {
      Pixmap pm_color = XCreatePixmap (state->dpy, state->window,
                                       (safe_width * 256), height,
                                       state->xgwa.depth);
      XImage *xim_color, *xim_mono;
      GC text_gc;
      XGCValues gcv;
      XftDraw *xftdraw;
      XftColor xft_fg;
      int x, y;

      /* Maybe this should be using XftDrawCreateBitmap instead of 
         XftDrawCreate to render the font with 1-bit hinting? */

      /* Create a full-depth pixmap and draw the text into it, fg/bg ~0/0. */
      gcv.foreground = 0;
      text_gc = XCreateGC (state->dpy, pm_color, GCForeground, &gcv);
      XFillRectangle (state->dpy, pm_color, text_gc, 0, 0,
                      (safe_width * 256), height);
      xftdraw = XftDrawCreate (state->dpy, pm_color,
                               state->xgwa.visual, state->xgwa.colormap);
      xft_fg.pixel = ~0L;
      xft_fg.color.red = xft_fg.color.green = xft_fg.color.blue = ~0L;
      for (i = 0; i < 256; i++)
        XftDrawStringUtf8 (xftdraw, &xft_fg, state->font,
                           i * safe_width, font->ascent,
                           (FcChar8 *) (string + i), 1);

      /* Retrieve a full-depth XImage. */
      xim_color = XGetImage (state->dpy, pm_color, 0, 0, 
                             i * safe_width, font->ascent,
                             state->xgwa.depth, ZPixmap);

      /* Convert it to a mono XImage. */
      xim_mono = XCreateImage (state->dpy, state->xgwa.visual,
                               1, XYPixmap, 0, 0,
                               i * safe_width, font->ascent,
                               8, 0);
      xim_mono->data = (char *)
        calloc(xim_mono->height, xim_mono->bytes_per_line);
      for (y = 0; y < xim_color->height; y++)
        for (x = 0; x < xim_color->width; x++)
          XPutPixel (xim_mono, x, y, XGetPixel (xim_color, x, y) ? 1 : 0);

      /* Copy mono ximage to mono pixmap */
      XFreeGC (state->dpy, text_gc);
      text_gc = XCreateGC (state->dpy, p, GCForeground, &gcv);
      XPutImage (state->dpy, p, text_gc, xim_mono, 0, 0, 0, 0,
                 (safe_width * 256), height);
      XFreeGC (state->dpy, text_gc);
      XDestroyImage (xim_color);
      XDestroyImage (xim_mono);
# if 0
      XWriteBitmapFile(state->dpy, "/tmp/tvfont.xbm", p, 
                       (safe_width * 256), height,
                       -1, -1);
# endif
    }

  /* Draw the cursor. */
  XFillRectangle (state->dpy, p, state->gc1,
                  (CURSOR_INDEX * safe_width), 1,
                  state->char_width,
                  state->char_height);

  state->font_bits = XGetImage (state->dpy, p, 0, 0,
                                (safe_width * 256), height, ~0L, XYPixmap);
  XFreePixmap (state->dpy, p);

  for (i = 0; i < 256; i++)
    state->chars[i] = make_character (state, i);
}


static p_char *
make_character (p_state *state, int c)
{
  p_char *pc = (p_char *) malloc (sizeof (*pc));
  pc->name = (unsigned char) c;
  pc->width =  state->scale * state->char_width;
  pc->height = state->scale * state->char_height;
  char_to_pixmap (state, pc, c);
  return pc;
}


static void
char_to_pixmap (p_state *state, p_char *pc, int c)
{
  Pixmap p = 0;
  GC gc;
#ifdef FUZZY_BORDER
  Pixmap p2 = 0;
  GC gc2;
#endif /* FUZZY_BORDER */
  int from, to;
  int x1, y;

  int safe_width = state->char_width + 1;

  int width  = state->scale * state->char_width;
  int height = state->scale * state->char_height;

  gc = state->gc1;
  p = XCreatePixmap (state->dpy, state->window, width, height, 1);
  XFillRectangle (state->dpy, p, state->gc0, 0, 0, width, height);
#ifdef FUZZY_BORDER
  gc2 = state->gc2;
  p2 = XCreatePixmap (state->dpy, state->window, width, height, 1);
  XFillRectangle (state->dpy, p2, state->gc0, 0, 0, width, height);
#endif /* FUZZY_BORDER */

  from = safe_width * c;
  to =   safe_width * (c + 1);

#if 0
  if (c > 75 && c < 150)
    {
      printf ("\n=========== %d (%c)\n", c, c);
      for (y = 0; y < state->char_height; y++)
        {
          for (x1 = from; x1 < to; x1++)
            printf (XGetPixel (state->font_bits, x1, y) ? "* " : ". ");
          printf ("\n");
        }
    }
#endif

  pc->blank_p = True;
  for (y = 0; y < state->char_height; y++)
    for (x1 = from; x1 < to; x1++)
      if (XGetPixel (state->font_bits, x1, y))
        {
          int xoff = state->scale / 2;
          int x2;
          for (x2 = x1; x2 < to; x2++)
            if (!XGetPixel (state->font_bits, x2, y))
              break;
          x2--;
          XDrawLine (state->dpy, p, gc,
                     (x1 - from) * state->scale + xoff, y * state->scale,
                     (x2 - from) * state->scale + xoff, y * state->scale);
#ifdef FUZZY_BORDER
          XDrawLine (state->dpy, p2, gc2,
                     (x1 - from) * state->scale + xoff, y * state->scale,
                     (x2 - from) * state->scale + xoff, y * state->scale);
#endif /* FUZZY_BORDER */
          x1 = x2;
          pc->blank_p = False;
        }

  /*  if (pc->blank_p && c == CURSOR_INDEX)
    abort();*/

  pc->pixmap = p;
#ifdef FUZZY_BORDER
  pc->pixmap2 = p2;
#endif /* FUZZY_BORDER */
}


/* Managing the display. 
 */

static void cursor_on_timer (XtPointer closure, XtIntervalId *id);
static void cursor_off_timer (XtPointer closure, XtIntervalId *id);

static Bool
set_cursor_1 (p_state *state, Bool on)
{
  p_cell *cell = &state->cells[state->grid_width * state->cursor_y
                              + state->cursor_x];
  p_char *cursor = state->chars[CURSOR_INDEX];
  int new_state = (on ? NORMAL : FADE);

  if (cell->p_char != cursor)
    cell->changed = True;

  if (cell->state != new_state)
    cell->changed = True;

  cell->p_char = cursor;
  cell->state = new_state;
  return cell->changed;
}

static void
set_cursor (p_state *state, Bool on)
{
  if (set_cursor_1 (state, on))
    {
      if (state->cursor_timer)
        XtRemoveTimeOut (state->cursor_timer);
      state->cursor_timer = 0;
      cursor_on_timer (state, 0);
    }
}


static void
cursor_off_timer (XtPointer closure, XtIntervalId *id)
{
  p_state *state = (p_state *) closure;
  XtAppContext app = XtDisplayToApplicationContext (state->dpy);
  set_cursor_1 (state, False);
  state->cursor_timer = XtAppAddTimeOut (app, state->cursor_blink,
                                         cursor_on_timer, closure);
}

static void
cursor_on_timer (XtPointer closure, XtIntervalId *id)
{
  p_state *state = (p_state *) closure;
  XtAppContext app = XtDisplayToApplicationContext (state->dpy);
  set_cursor_1 (state, True);
  state->cursor_timer = XtAppAddTimeOut (app, 2 * state->cursor_blink,
                                         cursor_off_timer, closure);
}


static void
clear (p_state *state)
{
  int x, y;
  state->cursor_x = 0;
  state->cursor_y = 0;
  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        if (cell->state == FLARE || cell->state == NORMAL)
          {
            cell->state = FADE;
            cell->changed = True;
          }
      }
  set_cursor (state, True);
}


static void
decay (p_state *state)
{
  int x, y;
  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        if (cell->state == FLARE)
          {
            cell->state = NORMAL;
            cell->changed = True;
          }
        else if (cell->state >= FADE)
          {
            cell->state++;
            if (cell->state >= state->ticks)
              cell->state = BLANK;
            cell->changed = True;
          }
      }
}


static void
scroll (p_state *state)
{
  int x, y;

  for (x = 0; x < state->grid_width; x++)
    {
      p_cell *from = 0, *to = 0;
      for (y = 1; y < state->grid_height; y++)
        {
          from = &state->cells[state->grid_width * y     + x];
          to   = &state->cells[state->grid_width * (y-1) + x];

          if ((from->state == FLARE || from->state == NORMAL) &&
              !from->p_char->blank_p)
            {
              *to = *from;
              to->state = NORMAL;  /* should be FLARE?  Looks bad... */
            }
          else
            {
              if (to->state == FLARE || to->state == NORMAL)
                to->state = FADE;
            }

          to->changed = True;
        }

      to = from;
      if (to && (to->state == FLARE || to->state == NORMAL))
        {
          to->state = FADE;
          to->changed = True;
        }
    }
  set_cursor (state, True);
}


static int
process_unicrud (p_state *state, int c)
{
  if ((c & 0xE0) == 0xC0) {        /* 110xxxxx: 11 bits, 2 bytes */
    state->unicruds = 1;
    state->unicrud[0] = c;
    state->escstate = 102;
  } else if ((c & 0xF0) == 0xE0) { /* 1110xxxx: 16 bits, 3 bytes */
    state->unicruds = 1;
    state->unicrud[0] = c;
    state->escstate = 103;
  } else if ((c & 0xF8) == 0xF0) { /* 11110xxx: 21 bits, 4 bytes */
    state->unicruds = 1;
    state->unicrud[0] = c;
    state->escstate = 104;
  } else if ((c & 0xFC) == 0xF8) { /* 111110xx: 26 bits, 5 bytes */
    state->unicruds = 1;
    state->unicrud[0] = c;
    state->escstate = 105;
  } else if ((c & 0xFE) == 0xFC) { /* 1111110x: 31 bits, 6 bytes */
    state->unicruds = 1;
    state->unicrud[0] = c;
    state->escstate = 106;
  } else if (state->unicruds == 0) {
    return c;
  } else {
    int total = state->escstate - 100;  /* see what I did there */
    if (state->unicruds < total) {
      /* Buffer more bytes of the UTF-8 sequence */
      state->unicrud[state->unicruds++] = c;
    }

    if (state->unicruds >= total) {
      /* Done! Convert it to Latin1 and print that. */
      char *s;
      state->unicrud[state->unicruds] = 0;
      s = utf8_to_latin1 ((const char *) state->unicrud, False);
      state->unicruds = 0;
      state->escstate = 0;
      if (s) {
        c = (unsigned char) s[0];
        free (s);
        return c;
      }
    }
  }
  return 0;
}


static void
print_char (p_state *state, int c)
{
  int cols = state->grid_width;
  int rows = state->grid_height;
  p_cell *cell = &state->cells[state->grid_width * state->cursor_y
			       + state->cursor_x];

  /* Start the cursor fading (in case we don't end up overwriting it.) */
  if (cell->state == FLARE || cell->state == NORMAL)
    {
      cell->state = FADE;
      cell->changed = True;
    }
  
#ifdef HAVE_FORKPTY
  if (state->pty_p) /* Only interpret VT100 sequences if running in pty-mode.
                       It would be nice if we could just interpret them all
                       the time, but that would require subprocesses to send
                       CRLF line endings instead of bare LF, so that's no good.
                     */
    {
      int i;
      int start, end;

      /* Mostly duplicated in apple2-main.c */

      switch (state->escstate)
	{
	case 0:
	  switch (c)
	    {
	    case 7: /* BEL */
	      /* Dummy case - we don't want the screensaver to beep */
              /* #### But maybe this should flash the screen? */
	      break;
	    case 8: /* BS */
	      if (state->cursor_x > 0)
		state->cursor_x--;
	      break;
	    case 9: /* HT */
	      if (state->cursor_x < cols - 8)
		{
		  state->cursor_x = (state->cursor_x & ~7) + 8;
		}
	      else
		{
		  state->cursor_x = 0;
		  if (state->cursor_y < rows - 1)
		    state->cursor_y++;
		  else
		    scroll (state);
		}
	      break;
	    case 10: /* LF */
# ifndef HAVE_FORKPTY
              state->cursor_x = 0;	/* No ptys on iPhone; assume CRLF. */
# endif
	    case 11: /* VT */
	    case 12: /* FF */
	      if(state->last_c == 13)
		{
		  cell->state = NORMAL;
		  cell->p_char = state->chars[state->bk];
		  cell->changed = True;
		}
	      if (state->cursor_y < rows - 1)
		state->cursor_y++;
	      else
		scroll (state);
	      break;
	    case 13: /* CR */
	      state->cursor_x = 0;
	      cell = &state->cells[cols * state->cursor_y];
	      if((cell->p_char == NULL) || (cell->p_char->name == CURSOR_INDEX))
		state->bk = ' ';
	      else
		state->bk = cell->p_char->name;
	      break;
	    case 14: /* SO */
	    case 15: /* SI */
              /* Dummy case - there is one and only one font. */
	      break;
	    case 24: /* CAN */
	    case 26: /* SUB */
	      /* Dummy case - these interrupt escape sequences, so
		 they don't do anything in this state */
	      break;
	    case 27: /* ESC */
	      state->escstate = 1;
	      break;
	    case 127: /* DEL */
	      /* Dummy case - this is supposed to be ignored */
	      break;
	    case 155: /* CSI */
	      state->escstate = 2;
	      for(i = 0; i < NPAR; i++)
		state->csiparam[i] = 0;
	      state->curparam = 0;
	      break;
	    default:

            PRINT: /* Come from states 102-106 */
              c = process_unicrud (state, c);
              if (! c)
                break;

              /* If the cursor is in column 39 and we print a character, then
                 that character shows up in column 39, and the cursor is no
                 longer visible on the screen (it's in "column 40".)  If
                 another character is printed, then that character shows up in
                 column 0, and the cursor moves to column 1.

                 This is empirically what xterm and gnome-terminal do, so that
                 must be the right thing.  (In xterm, the cursor vanishes,
                 whereas; in gnome-terminal, the cursor overprints the
                 character in col 39.)
               */
	      cell->state = FLARE;
	      cell->p_char = state->chars[c];
	      cell->changed = True;
	      state->cursor_x++;

	      if (c != ' ' && cell->p_char->blank_p)
		cell->p_char = state->chars[CURSOR_INDEX];

	      if (state->cursor_x >= cols - 1 /*####*/)
		{
		  state->cursor_x = 0;
		  if (state->cursor_y >= rows - 1)
		    scroll (state);
		  else
		    state->cursor_y++;
		}
	      break;
	    }
	  break;
	case 1:
	  switch (c)
	    {
	    case 24: /* CAN */
	    case 26: /* SUB */
	      state->escstate = 0;
	      break;
	    case 'c': /* Reset */
	      clear (state);
	      state->escstate = 0;
	      break;
	    case 'D': /* Linefeed */
	      if (state->cursor_y < rows - 1)
		state->cursor_y++;
	      else
		scroll (state);
	      state->escstate = 0;
	      break;
	    case 'E': /* Newline */
	      state->cursor_x = 0;
	      state->escstate = 0;
	      break;
	    case 'M': /* Reverse newline */
	      if (state->cursor_y > 0)
		state->cursor_y--;
	      state->escstate = 0;
	      break;
	    case '7': /* Save state */
	      state->saved_x = state->cursor_x;
	      state->saved_y = state->cursor_y;
	      state->escstate = 0;
	      break;
	    case '8': /* Restore state */
	      state->cursor_x = state->saved_x;
	      state->cursor_y = state->saved_y;
	      state->escstate = 0;
	      break;
	    case '[': /* CSI */
	      state->escstate = 2;
	      for(i = 0; i < NPAR; i++)
		state->csiparam[i] = 0;
	      state->curparam = 0;
	      break;
            case '%': /* Select charset */
              /* @: Select default (ISO 646 / ISO 8859-1)
                 G: Select UTF-8
                 8: Select UTF-8 (obsolete)

                 We can just ignore this and always process UTF-8, I think?
                 We must still catch the last byte, though.
               */
	    case '(':
	    case ')':
	      /* I don't support different fonts either - see above
		 for SO and SI */
	      state->escstate = 3;
	      break;
	    default:
	      /* Escape sequences not supported:
	       * 
	       * H - Set tab stop
	       * Z - Terminal identification
	       * > - Keypad change
	       * = - Other keypad change
	       * ] - OS command
	       */
	      state->escstate = 0;
	      break;
	    }
	  break;
	case 2:
	  switch (c)
	    {
	    case 24: /* CAN */
	    case 26: /* SUB */
	      state->escstate = 0;
	      break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
	      if (state->curparam < NPAR)
		state->csiparam[state->curparam] = (state->csiparam[state->curparam] * 10) + (c - '0');
	      break;
	    case ';':
	      state->csiparam[++state->curparam] = 0;
	      break;
	    case '[':
	      state->escstate = 3;
	      break;
	    case '@':
	      for (i = 0; i < state->csiparam[0]; i++)
		{
		  if(++state->cursor_x > cols)
		    {
		      state->cursor_x = 0;
		      if (state->cursor_y < rows - 1)
			state->cursor_y++;
		      else
			scroll (state);
		    }
		  cell = &state->cells[cols * state->cursor_y + state->cursor_x];
		  if (cell->state == FLARE || cell->state == NORMAL)
		    {
		      cell->state = FADE;
		      cell->changed = True;
		    }
		}
	      state->escstate = 0;
	      break;
	    case 'F':
	      state->cursor_x = 0;
	    case 'A':
	      if (state->csiparam[0] == 0)
		state->csiparam[0] = 1;
	      if ((state->cursor_y -= state->csiparam[0]) < 0)
		state->cursor_y = 0;
	      state->escstate = 0;
	      break;
	    case 'E':
	      state->cursor_x = 0;
	    case 'e':
	    case 'B':
	      if (state->csiparam[0] == 0)
		state->csiparam[0] = 1;
	      if ((state->cursor_y += state->csiparam[0]) >= rows - 1 /*####*/)
		state->cursor_y = rows - 1;
	      state->escstate = 0;
	      break;
	    case 'a':
	    case 'C':
	      if (state->csiparam[0] == 0)
		state->csiparam[0] = 1;
	      if ((state->cursor_x += state->csiparam[0]) >= cols - 1 /*####*/)
		state->cursor_x = cols - 1;
	      state->escstate = 0;
	      break;
	    case 'D':
	      if (state->csiparam[0] == 0)
		state->csiparam[0] = 1;
	      if ((state->cursor_x -= state->csiparam[0]) < 0)
		state->cursor_x = 0;
	      state->escstate = 0;
	      break;
	    case 'd':
	      if ((state->cursor_y = (state->csiparam[0] - 1)) >= rows - 1 /*####*/)
		state->cursor_y = rows - 1;
	      state->escstate = 0;
	      break;
	    case '`':
	    case 'G':
	      if ((state->cursor_x = (state->csiparam[0] - 1)) >= cols - 1 /*####*/)
		state->cursor_x = cols - 1;
	      state->escstate = 0;
	      break;
	    case 'f':
	    case 'H':
	      if ((state->cursor_y = (state->csiparam[0] - 1)) >= rows - 1 /*####*/)
		state->cursor_y = rows - 1;
	      if ((state->cursor_x = (state->csiparam[1] - 1)) >= cols - 1 /*####*/)
		state->cursor_x = cols - 1;
	      if(state->cursor_y < 0)
		state->cursor_y = 0;
	      if(state->cursor_x < 0)
		state->cursor_x = 0;
	      state->escstate = 0;
	      break;
	    case 'J':
	      start = 0;
	      end = rows * cols;
	      if (state->csiparam[0] == 0)
		start = cols * state->cursor_y + state->cursor_x;
	      if (state->csiparam[0] == 1)
		end = cols * state->cursor_y + state->cursor_x;
	      for (i = start; i < end; i++)
		{
		  cell = &state->cells[i];
		  if (cell->state == FLARE || cell->state == NORMAL)
		    {
		      cell->state = FADE;
		      cell->changed = True;
		    }
		}
	      set_cursor (state, True);
	      state->escstate = 0;
	      break;
	    case 'K':
	      start = 0;
	      end = cols;
	      if (state->csiparam[0] == 0)
		start = state->cursor_x;
	      if (state->csiparam[1] == 1)
		end = state->cursor_x;
	      for (i = start; i < end; i++)
		{
		  if (cell->state == FLARE || cell->state == NORMAL)
		    {
		      cell->state = FADE;
		      cell->changed = True;
		    }
		  cell++;
		}
	      state->escstate = 0;
	      break;
            case 'm': /* Set attributes unimplemented (bold, blink, rev) */
              state->escstate = 0;
              break;
	    case 's': /* Save position */
	      state->saved_x = state->cursor_x;
	      state->saved_y = state->cursor_y;
	      state->escstate = 0;
	      break;
	    case 'u': /* Restore position */
	      state->cursor_x = state->saved_x;
	      state->cursor_y = state->saved_y;
	      state->escstate = 0;
	      break;
	    case '?': /* DEC Private modes */
	      if ((state->curparam != 0) || (state->csiparam[0] != 0))
		state->escstate = 0;
	      break;
	    default:
	      /* Known unsupported CSIs:
	       *
	       * L - Insert blank lines
	       * M - Delete lines (I don't know what this means...)
	       * P - Delete characters
	       * X - Erase characters (difference with P being...?)
	       * c - Terminal identification
	       * g - Clear tab stop(s)
	       * h - Set mode (Mainly due to its complexity and lack of good
                     docs)
	       * l - Clear mode
	       * m - Set mode (Phosphor is, per defenition, green on black)
	       * n - Status report
	       * q - Set keyboard LEDs
	       * r - Set scrolling region (too exhausting - noone uses this,
                     right?)
	       */
	      state->escstate = 0;
	      break;
	    }
	  break;
	case 3:
	  state->escstate = 0;
	  break;

        case 102:	/* states 102-106 are for UTF-8 decoding */
        case 103:
        case 104:
        case 105:
        case 106:
          goto PRINT;

        default:
          abort();
	}
      set_cursor (state, True);
    }
  else
#endif /* HAVE_FORKPTY */
    {
      if (c == '\t') c = ' ';   /* blah. */

      if (c == '\r' || c == '\n')  /* handle CR, LF, or CRLF as "new line". */
	{
	  if (c == '\n' && state->last_c == '\r')
	    ;   /* CRLF -- do nothing */
	  else
	    {
	      state->cursor_x = 0;
	      if (state->cursor_y == rows - 1)
		scroll (state);
	      else
		state->cursor_y++;
	    }
	}
      else if (c == '\014')
	{
	  clear (state);
	}
      else
	{
          c = process_unicrud (state, c);
          if (!c) return;

	  cell->state = FLARE;
	  cell->p_char = state->chars[c];
	  cell->changed = True;
	  state->cursor_x++;

	  if (c != ' ' && cell->p_char->blank_p)
	    cell->p_char = state->chars[CURSOR_INDEX];

	  if (state->cursor_x >= cols - 1)
	    {
	      state->cursor_x = 0;
	      if (state->cursor_y >= rows - 1)
		scroll (state);
	      else
		state->cursor_y++;
	    }
	}
      set_cursor (state, True);
    }

  state->last_c = c;
}


static void
update_display (p_state *state, Bool changed_only)
{
  int x, y;

  for (y = 0; y < state->grid_height; y++)
    for (x = 0; x < state->grid_width; x++)
      {
        p_cell *cell = &state->cells[state->grid_width * y + x];
        int width, height, tx, ty;

        if (changed_only && !cell->changed)
          continue;

        width  = state->char_width  * state->scale;
        height = state->char_height * state->scale;
        tx = x * width  + state->xmargin;
        ty = y * height + state->ymargin;

        if (cell->state == BLANK || cell->p_char->blank_p)
          {
            XFillRectangle (state->dpy, state->window, state->gcs[BLANK],
                            tx, ty, width, height);
          }
        else
          {
#ifdef FUZZY_BORDER
            GC gc1 = state->gcs[cell->state];
            GC gc2 = ((cell->state + 2) < state->ticks
                      ? state->gcs[cell->state + 2]
                      : 0);
            GC gc3 = (gc2 ? gc2 : gc1);
            if (gc3)
              XCopyPlane (state->dpy, cell->p_char->pixmap, state->window, gc3,
                          0, 0, width, height, tx, ty, 1L);
            if (gc2)
              {
                XSetClipMask (state->dpy, gc1, cell->p_char->pixmap2);
                XSetClipOrigin (state->dpy, gc1, tx, ty);
                XFillRectangle (state->dpy, state->window, gc1,
                                tx, ty, width, height);
                XSetClipMask (state->dpy, gc1, None);
              }
#else /* !FUZZY_BORDER */

            XCopyPlane (state->dpy,
                        cell->p_char->pixmap, state->window,
                        state->gcs[cell->state],
                        0, 0, width, height, tx, ty, 1L);

#endif /* !FUZZY_BORDER */
          }

        cell->changed = False;
      }
}


static unsigned long
phosphor_draw (Display *dpy, Window window, void *closure)
{
  p_state *state = (p_state *) closure;
  int c;
  update_display (state, True);
  decay (state);

  c = textclient_getc (state->tc);
  if (c > 0) 
    print_char (state, c);

  return state->delay;
}


static void
phosphor_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  p_state *state = (p_state *) closure;
  Bool changed_p = resize_grid (state);

  if (! changed_p) return;

  textclient_reshape (state->tc,
                      w - state->xmargin * 2,
                      h - state->ymargin * 2,
                      state->grid_width  - 1,
                      state->grid_height - 1,
                      0);
}


static Bool
phosphor_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  p_state *state = (p_state *) closure;

  if (event->xany.type == Expose)
    update_display (state, False);
  else if (event->xany.type == KeyPress)
    return textclient_putc (state->tc, &event->xkey);
  return False;
}

static void
phosphor_free (Display *dpy, Window window, void *closure)
{
  p_state *state = (p_state *) closure;
  int i;

  textclient_close (state->tc);
  if (state->cursor_timer)
    XtRemoveTimeOut (state->cursor_timer);

  if (state->gc0) XFreeGC (dpy, state->gc0);
  if (state->gc1) XFreeGC (dpy, state->gc1);
#ifdef FUZZY_BORDER
  if (state->gc2) XFreeGC (dpy, state->gc2);
#endif /* FUZZY_BORDER */
  for (i = 0; i < state->ticks; i++)
    if (state->gcs[i]) XFreeGC (dpy, state->gcs[i]);
  free (state->gcs);
  for (i = 0; i < 256; i++) {
    XFreePixmap (dpy, state->chars[i]->pixmap);
#ifdef FUZZY_BORDER
    XFreePixmap (dpy, state->chars[i]->pixmap2);
#endif /* FUZZY_BORDER */
    free (state->chars[i]);
  }
  XDestroyImage (state->font_bits);
  free (state->chars);
  free (state->cells);
  free (state);
}



static const char *phosphor_defaults [] = {
/*  ".lowrez:                true",*/
  ".background:		   Black",
  ".foreground:		   #00FF00",
  "*fpsSolid:		   true",
#if defined(BUILTIN_FONT)
  "*font:		   (builtin)",
#elif defined(HAVE_COCOA)
  "*font:		   Monaco 15",
#else
  "*font:		   fixed",
#endif
  "*phosphorScale:	   6",
  "*ticks:		   20",
  "*delay:		   50000",
  "*cursor:		   333",
  "*program:		   xscreensaver-text",
  "*relaunch:		   5",
  "*metaSendsESC:	   True",
  "*swapBSDEL:		   True",
#ifdef HAVE_FORKPTY
  "*usePty:                True",
#else  /* !HAVE_FORKPTY */
  "*usePty:                False",
#endif /* !HAVE_FORKPTY */
  0
};

static XrmOptionDescRec phosphor_options [] = {
  { "-font",		".font",		XrmoptionSepArg, 0 },
  { "-scale",		".phosphorScale",	XrmoptionSepArg, 0 },
  { "-ticks",		".ticks",		XrmoptionSepArg, 0 },
  { "-delay",		".delay",		XrmoptionSepArg, 0 },
  { "-program",		".program",		XrmoptionSepArg, 0 },
  { "-pipe",		".usePty",		XrmoptionNoArg, "False" },
  { "-pty",		".usePty",		XrmoptionNoArg, "True"  },
  { "-meta",		".metaSendsESC",	XrmoptionNoArg, "False" },
  { "-esc",		".metaSendsESC",	XrmoptionNoArg, "True"  },
  { "-bs",		".swapBSDEL",		XrmoptionNoArg, "False" },
  { "-del",		".swapBSDEL",		XrmoptionNoArg, "True"  },
  { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Phosphor", phosphor)
